// Copyright 2026 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51

// Cluster-local streaming update engine for online softmax merge state.
module online_merge_update_engine #(
  parameter int unsigned AddrWidth = 32,
  parameter int unsigned DataWidth = 64,
  parameter type tcdm_req_t = logic,
  parameter type tcdm_rsp_t = logic
) (
  input  logic                 clk_i,
  input  logic                 rst_ni,

  input  logic [AddrWidth-1:0] src_m_old_i,
  input  logic [AddrWidth-1:0] src_l_old_i,
  input  logic [AddrWidth-1:0] src_o_old_i,
  input  logic [AddrWidth-1:0] src_m_tile_i,
  input  logic [AddrWidth-1:0] src_l_tile_i,
  input  logic [AddrWidth-1:0] src_o_tile_i,
  input  logic [AddrWidth-1:0] dst_m_i,
  input  logic [AddrWidth-1:0] dst_l_i,
  input  logic [AddrWidth-1:0] dst_o_i,
  input  logic [31:0]          n_i,
  input  logic [31:0]          d_i,
  input  logic [31:0]          stride_i,
  input  logic                 start_i,
  input  logic                 clear_done_i,

  output logic                 busy_o,
  output logic                 done_o,
  output logic                 error_o,

  output tcdm_req_t            tcdm_req_o,
  input  tcdm_rsp_t            tcdm_rsp_i
);

  localparam int unsigned ByteWidth = DataWidth / 8;
  typedef logic [AddrWidth-1:0] addr_t;
  typedef logic [DataWidth-1:0] data_t;
  typedef logic [ByteWidth-1:0] strb_t;

  typedef enum logic [2:0] {
    IDLE,
    LOAD_SCALAR,
    COMPUTE_SCALAR,
    STORE_SCALAR,
    UPDATE_VECTOR,
    DONE,
    ERROR
  } state_e;

  typedef enum logic [3:0] {
    RD_M_OLD,
    RD_L_OLD,
    RD_M_TILE,
    RD_L_TILE,
    WR_M_OUT,
    WR_L_OUT,
    RD_O_OLD,
    RD_O_TILE,
    WR_O_OUT
  } op_e;

  state_e state_q;
  logic done_q, error_q;
  logic req_valid_q, req_write_q, read_pending_q;
  addr_t req_addr_q;
  data_t req_wdata_q;
  strb_t req_strb_q;
  op_e op_q;

  logic [31:0] row_q, elem_q;
  logic [2:0] scalar_idx_q;
  logic [1:0] store_idx_q;
  logic [1:0] vector_idx_q;
  logic [31:0] m_old_q, l_old_q, m_tile_q, l_tile_q;
  logic [31:0] m_new_q, l_new_q, o_old_q, o_tile_q, o_new_q;

  function automatic logic [31:0] pick_fp32(input data_t data, input addr_t addr);
    pick_fp32 = addr[2] ? data[63:32] : data[31:0];
  endfunction

  function automatic data_t pack_fp32(input logic [31:0] value, input addr_t addr);
    pack_fp32 = '0;
    if (addr[2]) begin
      pack_fp32[63:32] = value;
    end else begin
      pack_fp32[31:0] = value;
    end
  endfunction

  function automatic strb_t fp32_strb(input addr_t addr);
    fp32_strb = addr[2] ? strb_t'(8'hf0) : strb_t'(8'h0f);
  endfunction

  function automatic addr_t scalar_addr(input addr_t base, input logic [31:0] row);
    scalar_addr = base + addr_t'(row << 2);
  endfunction

  function automatic addr_t vector_addr(
    input addr_t base,
    input logic [31:0] row,
    input logic [31:0] elem,
    input logic [31:0] stride,
    input logic [31:0] d
  );
    logic [31:0] eff_stride;
    eff_stride = (stride == 32'd0) ? (d << 2) : stride;
    vector_addr = base + addr_t'(row * eff_stride) + addr_t'(elem << 2);
  endfunction

  function automatic logic is_aligned(input addr_t addr);
    is_aligned = (addr[1:0] == 2'b00);
  endfunction

  function automatic logic valid_cfg;
    logic stride_aligned;
    begin
      stride_aligned = (stride_i == 32'd0) || (stride_i[1:0] == 2'b00);
      valid_cfg = (n_i != 32'd0) && (d_i != 32'd0) && stride_aligned &&
          is_aligned(src_m_old_i) && is_aligned(src_l_old_i) &&
          is_aligned(src_o_old_i) && is_aligned(src_m_tile_i) &&
          is_aligned(src_l_tile_i) && is_aligned(src_o_tile_i) &&
          is_aligned(dst_m_i) && is_aligned(dst_l_i) && is_aligned(dst_o_i);
    end
  endfunction

  function automatic logic fp32_lt(input logic [31:0] lhs, input logic [31:0] rhs);
    logic lhs_sign, rhs_sign;
    begin
      lhs_sign = lhs[31];
      rhs_sign = rhs[31];
      if (lhs == rhs) begin
        fp32_lt = 1'b0;
      end else if (lhs_sign != rhs_sign) begin
        fp32_lt = lhs_sign;
      end else if (lhs_sign) begin
        fp32_lt = lhs > rhs;
      end else begin
        fp32_lt = lhs < rhs;
      end
    end
  endfunction

  function automatic logic [31:0] fp32_half(input logic [31:0] value);
    fp32_half = value;
    if (value[30:23] != 8'd0) begin
      fp32_half[30:23] = value[30:23] - 8'd1;
    end
  endfunction

  function automatic logic [31:0] fp32_add_pow2_aligned(
    input logic [31:0] lhs,
    input logic [31:0] rhs
  );
    logic [24:0] sum;
    begin
      sum = {1'b1, lhs[22:0]} + {1'b1, rhs[22:0]};
      fp32_add_pow2_aligned = lhs;
      if (sum[24]) begin
        fp32_add_pow2_aligned[30:23] = lhs[30:23] + 8'd1;
        fp32_add_pow2_aligned[22:0] = sum[23:1];
      end else begin
        fp32_add_pow2_aligned[22:0] = sum[22:0];
      end
    end
  endfunction

  function automatic void compute_scalar_merge(
    input  logic [31:0] m_old_bits,
    input  logic [31:0] l_old_bits,
    input  logic [31:0] m_tile_bits,
    input  logic [31:0] l_tile_bits,
    output logic [31:0] m_new_bits,
    output logic [31:0] l_new_bits
  );
    begin
      if (l_tile_bits == 32'd0) begin
        m_new_bits = m_old_bits;
        l_new_bits = l_old_bits;
      end else if (l_old_bits == 32'd0) begin
        m_new_bits = m_tile_bits;
        l_new_bits = l_tile_bits;
      end else begin
        m_new_bits = fp32_lt(m_old_bits, m_tile_bits) ? m_tile_bits : m_old_bits;
        l_new_bits = fp32_add_pow2_aligned(l_old_bits, l_tile_bits);
      end
    end
  endfunction

  function automatic logic [31:0] compute_vector_merge(
    input logic [31:0] o_old_bits,
    input logic [31:0] o_tile_bits,
    input logic [31:0] m_old_bits,
    input logic [31:0] l_old_bits,
    input logic [31:0] m_tile_bits,
    input logic [31:0] l_tile_bits,
    input logic [31:0] m_new_bits,
    input logic [31:0] l_new_bits
  );
    logic unused;
    begin
      unused = ^{m_old_bits, m_tile_bits, m_new_bits, l_new_bits};
      if (l_tile_bits == 32'd0) begin
        compute_vector_merge = o_old_bits;
      end else if (l_old_bits == 32'd0) begin
        compute_vector_merge = o_tile_bits;
      end else begin
        compute_vector_merge = fp32_add_pow2_aligned(fp32_half(o_old_bits), fp32_half(o_tile_bits));
      end
    end
  endfunction

  function automatic addr_t scalar_read_addr(input logic [2:0] idx);
    unique case (idx)
      3'd0: scalar_read_addr = scalar_addr(src_m_old_i, row_q);
      3'd1: scalar_read_addr = scalar_addr(src_l_old_i, row_q);
      3'd2: scalar_read_addr = scalar_addr(src_m_tile_i, row_q);
      default: scalar_read_addr = scalar_addr(src_l_tile_i, row_q);
    endcase
  endfunction

  function automatic op_e scalar_read_op(input logic [2:0] idx);
    unique case (idx)
      3'd0: scalar_read_op = RD_M_OLD;
      3'd1: scalar_read_op = RD_L_OLD;
      3'd2: scalar_read_op = RD_M_TILE;
      default: scalar_read_op = RD_L_TILE;
    endcase
  endfunction

  always_comb begin
    tcdm_req_o = '0;
    tcdm_req_o.q_valid = req_valid_q;
    tcdm_req_o.q.addr = req_addr_q;
    tcdm_req_o.q.write = req_write_q;
    tcdm_req_o.q.amo = reqrsp_pkg::AMONone;
    tcdm_req_o.q.data = req_wdata_q;
    tcdm_req_o.q.strb = req_strb_q;
    tcdm_req_o.q.user = '0;
  end

  assign busy_o = (state_q != IDLE) && (state_q != DONE) && (state_q != ERROR);
  assign done_o = done_q;
  assign error_o = error_q;

  always_ff @(posedge clk_i or negedge rst_ni) begin
    if (!rst_ni) begin
      state_q <= IDLE;
      done_q <= 1'b0;
      error_q <= 1'b0;
      req_valid_q <= 1'b0;
      req_write_q <= 1'b0;
      read_pending_q <= 1'b0;
      req_addr_q <= '0;
      req_wdata_q <= '0;
      req_strb_q <= '0;
      op_q <= RD_M_OLD;
      row_q <= '0;
      elem_q <= '0;
      scalar_idx_q <= '0;
      store_idx_q <= '0;
      vector_idx_q <= '0;
      m_old_q <= '0;
      l_old_q <= '0;
      m_tile_q <= '0;
      l_tile_q <= '0;
      m_new_q <= '0;
      l_new_q <= '0;
      o_old_q <= '0;
      o_tile_q <= '0;
      o_new_q <= '0;
    end else begin
      if (clear_done_i) begin
        done_q <= 1'b0;
        error_q <= 1'b0;
        if ((state_q == DONE) || (state_q == ERROR)) begin
          state_q <= IDLE;
        end
      end

      if (req_valid_q && tcdm_rsp_i.q_ready) begin
        req_valid_q <= 1'b0;
        if (!req_write_q) begin
          read_pending_q <= 1'b1;
        end
      end

      if (read_pending_q && tcdm_rsp_i.p_valid) begin
        read_pending_q <= 1'b0;
        unique case (op_q)
          RD_M_OLD:  m_old_q <= pick_fp32(tcdm_rsp_i.p.data, req_addr_q);
          RD_L_OLD:  l_old_q <= pick_fp32(tcdm_rsp_i.p.data, req_addr_q);
          RD_M_TILE: m_tile_q <= pick_fp32(tcdm_rsp_i.p.data, req_addr_q);
          RD_L_TILE: l_tile_q <= pick_fp32(tcdm_rsp_i.p.data, req_addr_q);
          RD_O_OLD:  o_old_q <= pick_fp32(tcdm_rsp_i.p.data, req_addr_q);
          RD_O_TILE: o_tile_q <= pick_fp32(tcdm_rsp_i.p.data, req_addr_q);
          default: ;
        endcase
      end

      unique case (state_q)
        IDLE: begin
          if (start_i) begin
            done_q <= 1'b0;
            error_q <= 1'b0;
            row_q <= '0;
            elem_q <= '0;
            scalar_idx_q <= '0;
            store_idx_q <= '0;
            vector_idx_q <= '0;
            if (valid_cfg()) begin
              state_q <= LOAD_SCALAR;
            end else begin
              error_q <= 1'b1;
              state_q <= ERROR;
            end
          end
        end

        LOAD_SCALAR: begin
          if (!req_valid_q && !read_pending_q && (scalar_idx_q < 3'd4)) begin
            req_valid_q <= 1'b1;
            req_write_q <= 1'b0;
            req_addr_q <= scalar_read_addr(scalar_idx_q);
            req_wdata_q <= '0;
            req_strb_q <= '0;
            op_q <= scalar_read_op(scalar_idx_q);
          end else if (!req_valid_q && !read_pending_q && (scalar_idx_q == 3'd4)) begin
            scalar_idx_q <= '0;
            state_q <= COMPUTE_SCALAR;
          end
          if (read_pending_q && tcdm_rsp_i.p_valid) begin
            scalar_idx_q <= scalar_idx_q + 3'd1;
          end
        end

        COMPUTE_SCALAR: begin
          compute_scalar_merge(m_old_q, l_old_q, m_tile_q, l_tile_q, m_new_q, l_new_q);
          store_idx_q <= '0;
          state_q <= STORE_SCALAR;
        end

        STORE_SCALAR: begin
          if (!req_valid_q && !read_pending_q && (store_idx_q < 2'd2)) begin
            req_valid_q <= 1'b1;
            req_write_q <= 1'b1;
            req_addr_q <= (store_idx_q == 2'd0) ? scalar_addr(dst_m_i, row_q) : scalar_addr(dst_l_i, row_q);
            req_wdata_q <= pack_fp32((store_idx_q == 2'd0) ? m_new_q : l_new_q,
                                     (store_idx_q == 2'd0) ? scalar_addr(dst_m_i, row_q) : scalar_addr(dst_l_i, row_q));
            req_strb_q <= fp32_strb((store_idx_q == 2'd0) ? scalar_addr(dst_m_i, row_q) : scalar_addr(dst_l_i, row_q));
            op_q <= (store_idx_q == 2'd0) ? WR_M_OUT : WR_L_OUT;
          end else if (req_valid_q && tcdm_rsp_i.q_ready) begin
            store_idx_q <= store_idx_q + 2'd1;
          end else if (!req_valid_q && !read_pending_q && (store_idx_q == 2'd2)) begin
            elem_q <= '0;
            vector_idx_q <= '0;
            state_q <= UPDATE_VECTOR;
          end
        end

        UPDATE_VECTOR: begin
          if (elem_q == d_i) begin
            if (row_q == (n_i - 32'd1)) begin
              done_q <= 1'b1;
              state_q <= DONE;
            end else begin
              row_q <= row_q + 32'd1;
              elem_q <= '0;
              scalar_idx_q <= '0;
              state_q <= LOAD_SCALAR;
            end
          end else if (!req_valid_q && !read_pending_q) begin
            unique case (vector_idx_q)
              2'd0: begin
                req_valid_q <= 1'b1;
                req_write_q <= 1'b0;
                req_addr_q <= vector_addr(src_o_old_i, row_q, elem_q, stride_i, d_i);
                req_wdata_q <= '0;
                req_strb_q <= '0;
                op_q <= RD_O_OLD;
              end
              2'd1: begin
                req_valid_q <= 1'b1;
                req_write_q <= 1'b0;
                req_addr_q <= vector_addr(src_o_tile_i, row_q, elem_q, stride_i, d_i);
                req_wdata_q <= '0;
                req_strb_q <= '0;
                op_q <= RD_O_TILE;
              end
              default: begin
                o_new_q <= compute_vector_merge(o_old_q, o_tile_q, m_old_q, l_old_q,
                                                m_tile_q, l_tile_q, m_new_q, l_new_q);
                req_valid_q <= 1'b1;
                req_write_q <= 1'b1;
                req_addr_q <= vector_addr(dst_o_i, row_q, elem_q, stride_i, d_i);
                req_wdata_q <= pack_fp32(compute_vector_merge(o_old_q, o_tile_q, m_old_q, l_old_q,
                                                              m_tile_q, l_tile_q, m_new_q, l_new_q),
                                         vector_addr(dst_o_i, row_q, elem_q, stride_i, d_i));
                req_strb_q <= fp32_strb(vector_addr(dst_o_i, row_q, elem_q, stride_i, d_i));
                op_q <= WR_O_OUT;
              end
            endcase
          end

          if (read_pending_q && tcdm_rsp_i.p_valid) begin
            vector_idx_q <= vector_idx_q + 2'd1;
          end else if (req_valid_q && req_write_q && tcdm_rsp_i.q_ready && (op_q == WR_O_OUT)) begin
            vector_idx_q <= '0;
            elem_q <= elem_q + 32'd1;
          end
        end

        DONE: begin
          if (start_i) begin
            done_q <= 1'b0;
            error_q <= 1'b0;
            row_q <= '0;
            elem_q <= '0;
            scalar_idx_q <= '0;
            store_idx_q <= '0;
            vector_idx_q <= '0;
            if (valid_cfg()) begin
              state_q <= LOAD_SCALAR;
            end else begin
              error_q <= 1'b1;
              state_q <= ERROR;
            end
          end
        end

        ERROR: begin
          if (start_i) begin
            done_q <= 1'b0;
            error_q <= 1'b0;
            row_q <= '0;
            elem_q <= '0;
            scalar_idx_q <= '0;
            store_idx_q <= '0;
            vector_idx_q <= '0;
            if (valid_cfg()) begin
              state_q <= LOAD_SCALAR;
            end else begin
              error_q <= 1'b1;
            end
          end
        end

        default: begin
          error_q <= 1'b1;
          state_q <= ERROR;
        end
      endcase
    end
  end

endmodule
