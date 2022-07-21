#include <ops.cuh>


#define ADAM 0
#define MOMENTUM 1
#define RMSPROP 2

#define ROW 0
#define COL 1
#define COL32 2
#define COL_TURING 3
#define COL_AMPERE 4


// We cannot call templated code from C, so we wrap the template in a C compatible call here if necessary.
// We use macro functions to expand all the different optimizers. Looks ugly, and is ugly, but its better than to 
// maintain all that boilerplate
//===================================================================================
//                               UNMANGLED CALLS
//===================================================================================

void estimateQuantiles_fp32(float *A, float *code, float offset, int n){ estimateQuantiles<float>(A, code, offset, n); }
void estimateQuantiles_fp16(half *A, float *code, float offset, int n){ estimateQuantiles<half>(A, code, offset, n); }


#define MAKE_FUNC_TRANSFORM(fbits, fsrc, ftrgt, ftranspose, dtype, src, target, transpose, bits) \
void transform_##fbits##_##fsrc##_to_##ftrgt##_##ftranspose(cublasLtHandle_t ltHandle, dtype *A, dtype *out, int dim1, int dim2) \
{ \
	transform<dtype, src, target, transpose, bits>(ltHandle, A, out, dim1, dim2); \
} \

MAKE_FUNC_TRANSFORM(8, row, col, n, int8_t, ROW, COL, false, 8);
MAKE_FUNC_TRANSFORM(8, row, row, n, int8_t, ROW, ROW, false, 8);
MAKE_FUNC_TRANSFORM(8, row, col32, n, int8_t, ROW, COL32, false, 8);
MAKE_FUNC_TRANSFORM(32, row, col32, n, int32_t, ROW, COL32, false, 32);
MAKE_FUNC_TRANSFORM(8, row, col_turing, n, int8_t, ROW, COL_TURING, false, 8);
MAKE_FUNC_TRANSFORM(8, row, col_ampere, n, int8_t, ROW, COL_AMPERE, false, 8);
MAKE_FUNC_TRANSFORM(8, col32, row, n, int8_t, COL32, ROW, false, 8);
MAKE_FUNC_TRANSFORM(32, col32, row, n, int32_t, COL32, ROW, false, 32);


#define MAKE_FUNC32(fname, oname, gtype, gbits) \
void fname##32bit_g##gbits(gtype *g, gtype *p, \
               float* state1, float* state2, float *unorm, float max_unorm, float param_norm, \
               const float beta1, const float beta2, const float eps, const float weight_decay, \
               const int step, const float lr, float gnorm_scale, const int n) \
{ optimizer32bit<gtype, oname>(g, p, state1, state2, unorm, max_unorm, param_norm, beta1, beta2, eps, weight_decay, step, lr, gnorm_scale, n); } \

MAKE_FUNC32(momentum, MOMENTUM, float, 32)
MAKE_FUNC32(momentum, MOMENTUM, half, 16)
MAKE_FUNC32(adam, ADAM, float, 32)
MAKE_FUNC32(adam, ADAM, half, 16)
MAKE_FUNC32(rmsprop, RMSPROP, float, 32)
MAKE_FUNC32(rmsprop, RMSPROP, half, 16)

#define MAKE_FUNC8(fname, oname, gtype, gbits) \
void fname##_static_8bit_g##gbits(gtype* p, gtype* g, unsigned char* state1, unsigned char* state2, \
								float *unorm, float max_unorm, float param_norm, \
                float beta1, float beta2, \
                float eps, int step, float lr,  \
                float* quantiles1, float* quantiles2, \
                float* max1, float* max2, float* new_max1, float* new_max2, \
                float weight_decay, float gnorm_scale, int n) \
{  \
	optimizerStatic8bit<gtype, oname>(g, p, state1, state2, unorm, max_unorm, param_norm, beta1, beta2, eps, step, lr, \
			                                  quantiles1, quantiles2, max1, max2, new_max1, new_max2, weight_decay, gnorm_scale, n); \
} \

MAKE_FUNC8(adam, ADAM, float, 32)
MAKE_FUNC8(adam, ADAM, half, 16)
MAKE_FUNC8(momentum, MOMENTUM, float, 32)
MAKE_FUNC8(momentum, MOMENTUM, half, 16)
MAKE_FUNC8(rmsprop, RMSPROP, float, 32)
MAKE_FUNC8(rmsprop, RMSPROP, half, 16)

#define MAKE_BLOCKWISE8(fname, optim_name, gtype, gbits) \
void fname##_8bit_blockwise_fp##gbits(gtype* p, gtype* g, \
                unsigned char* state1, unsigned char* state2, float beta1, float beta2, float eps, int step, float lr, \
                float* quantiles1, float* quantiles2, float* absmax1, float* absmax2, float weight_decay, const float gnorm_scale, int n)\
{	optimizerStatic8bitBlockwise<gtype, optim_name>(p, g, state1, state2, beta1, beta2, eps, step, lr, quantiles1, quantiles2, absmax1, absmax2, weight_decay, gnorm_scale, n); }\

MAKE_BLOCKWISE8(adam, ADAM, half, 16)
MAKE_BLOCKWISE8(adam, ADAM, float, 32)
MAKE_BLOCKWISE8(momentum, MOMENTUM, half, 16)
MAKE_BLOCKWISE8(momentum, MOMENTUM, float, 32)
MAKE_BLOCKWISE8(rmsprop, RMSPROP, half, 16)
MAKE_BLOCKWISE8(rmsprop, RMSPROP, float, 32)


void percentileClipping_g32(float * g, float *gnorm_vec, int step, const int n){ percentileClipping<float>(g, gnorm_vec, step, n); }
void percentileClipping_g16(half * g, float *gnorm_vec, int step, const int n){ percentileClipping<half>(g, gnorm_vec, step, n); }

void quantizeBlockwise_fp16(float * code, half *A, float *absmax, unsigned char *out, const int n){ quantizeBlockwise<half, 0>(code, A, absmax, out, NULL, 0, n); }
void quantizeBlockwise_fp32(float * code, float *A, float *absmax, unsigned char *out, const int n){ quantizeBlockwise<float, 0>(code, A, absmax, out, NULL, 0, n); }
void quantizeBlockwise_stochastic_fp16(float * code, half *A, float *absmax, unsigned char *out, float* rand, int rand_offset, const int n){ quantizeBlockwise<half, 1>(code, A, absmax, out, rand, rand_offset, n); }
void quantizeBlockwise_stochastic_fp32(float * code, float *A, float *absmax, unsigned char *out, float* rand, int rand_offset, const int n){ quantizeBlockwise<float, 1>(code, A, absmax, out, rand, rand_offset, n); }

void dequantizeBlockwise_fp16(float *code, unsigned char *A, float *absmax, half *out, int blocksize, const int n){ dequantizeBlockwise<half>(code, A, absmax, out, blocksize, n); } \
void dequantizeBlockwise_fp32(float *code, unsigned char *A, float *absmax, float *out, int blocksize, const int n){ dequantizeBlockwise<float>(code, A, absmax, out, blocksize, n); }

void transform_row2col32(char * A, char *out, int rows, int cols){ transformRowToFormat<COL32, 0>(A, out, rows, cols); }
void transform_row2col32T(char * A, char *out, int rows, int cols){ transformRowToFormat<COL32, 1>(A, out, rows, cols); }
void transform_row2turing(char * A, char *out, int rows, int cols){ transformRowToFormat<COL_TURING, 0>(A, out, rows, cols); }
void transform_row2turingT(char * A, char *out, int rows, int cols){ transformRowToFormat<COL_TURING, 1>(A, out, rows, cols); }
void transform_row2ampere(char * A, char *out, int rows, int cols){ transformRowToFormat<COL_AMPERE, 0>(A, out, rows, cols); }
void transform_row2ampereT(char * A, char *out, int rows, int cols){ transformRowToFormat<COL_AMPERE, 1>(A, out, rows, cols); }

 int igemmlt_turing_32(cublasLtHandle_t ltHandle, int m, int n, int k, const int8_t *A, const int8_t *B, void *C, float *row_scale, int lda, int ldb, int ldc)
	{ return igemmlt<COL_TURING, 32, 0>(ltHandle, m, n, k, A, B, C, row_scale, lda, ldb, ldc); }

 int igemmlt_turing_8(cublasLtHandle_t ltHandle, int m, int n, int k, const int8_t *A, const int8_t *B, void *C, float *row_scale, int lda, int ldb, int ldc)
	{ return igemmlt<COL_TURING, 8, 0>(ltHandle, m, n, k, A, B, C, row_scale, lda, ldb, ldc); }

 int igemmlt_turing_8_rowscale(cublasLtHandle_t ltHandle, int m, int n, int k, const int8_t *A, const int8_t *B, void *C, float *row_scale, int lda, int ldb, int ldc)
	{ return igemmlt<COL_TURING, 8, 1>(ltHandle, m, n, k, A, B, C, row_scale, lda, ldb, ldc); }

 int igemmlt_ampere_32(cublasLtHandle_t ltHandle, int m, int n, int k, const int8_t *A, const int8_t *B, void *C, float *row_scale, int lda, int ldb, int ldc)
	{ return igemmlt<COL_AMPERE, 32, 0>(ltHandle, m, n, k, A, B, C, row_scale, lda, ldb, ldc); }

 int igemmlt_ampere_8(cublasLtHandle_t ltHandle, int m, int n, int k, const int8_t *A, const int8_t *B, void *C, float *row_scale, int lda, int ldb, int ldc)
	{ return igemmlt<COL_AMPERE, 8, 0>(ltHandle, m, n, k, A, B, C, row_scale, lda, ldb, ldc); }

 int igemmlt_ampere_8_rowscale(cublasLtHandle_t ltHandle, int m, int n, int k, const int8_t *A, const int8_t *B, void *C, float *row_scale, int lda, int ldb, int ldc)
	{ return igemmlt<COL_AMPERE, 8, 1>(ltHandle, m, n, k, A, B, C, row_scale, lda, ldb, ldc); }

void spmm_coo_very_sparse_naive_fp16(int *max_count, int *max_idx, int *offset_rowidx, int *rowidx, int *colidx, half *values, half *B, half *out, float *dequant_stats, int nnz_rows, int nnz, int rowsA, int rowsB, int colsB)
{ spmm_coo_very_sparse_naive<half, 16>(max_count, max_idx, offset_rowidx, rowidx, colidx, values, B, out, dequant_stats, nnz_rows, nnz, rowsA, rowsB, colsB); }

void spmm_coo_very_sparse_naive_int8(int *max_count, int *max_idx, int *offset_rowidx, int *rowidx, int *colidx, half *values, signed char *B, half *out, float *dequant_stats, int nnz_rows, int nnz, int rowsA, int rowsB, int colsB)
{ spmm_coo_very_sparse_naive<signed char, 8>(max_count, max_idx, offset_rowidx, rowidx, colidx, values, B, out, dequant_stats, nnz_rows, nnz, rowsA, rowsB, colsB); }

extern "C"
{
	void cestimate_quantiles_fp32(float *A, float *code, float offset, int n){ estimateQuantiles_fp32(A, code, offset, n); }
	void cestimate_quantiles_fp16(half *A, float *code, float offset, int n){ estimateQuantiles_fp16(A, code, offset, n); }
	void cquantize(float *code, float *A, unsigned char *out, int n){ quantize(code, A, out, n); }
	void cdequantize(float *code, unsigned char *A, float *out, int n){ dequantize(code, A, out, n); }
  void cquantize_blockwise_fp16(float * code, half *A, float *absmax, unsigned char *out, const int n){ quantizeBlockwise_fp16(code, A, absmax, out, n); }
  void cquantize_blockwise_fp32(float * code, float *A, float *absmax, unsigned char *out, const int n){ quantizeBlockwise_fp32(code, A, absmax, out, n); }
  void cquantize_blockwise_stochastic_fp16(float * code, half *A, float *absmax, unsigned char *out, float *rand, int rand_offset, const int n){ quantizeBlockwise_stochastic_fp16(code, A, absmax, out, rand, rand_offset, n); }
  void cquantize_blockwise_stochastic_fp32(float * code, float *A, float *absmax, unsigned char *out, float *rand, int rand_offset, const int n){ quantizeBlockwise_stochastic_fp32(code, A, absmax, out, rand, rand_offset, n); }

  void cdequantize_blockwise_fp16(float *code, unsigned char *A, float *absmax, half *out, int blocksize, const int n){ dequantizeBlockwise_fp16(code, A, absmax, out, blocksize, n); }
  void cdequantize_blockwise_fp32(float *code, unsigned char *A, float *absmax, float *out, int blocksize, const int n){ dequantizeBlockwise_fp32(code, A, absmax, out, blocksize, n); }

	#define MAKE_CFUNC32(name, gtype, gbits) \
	void c##name##32bit_g##gbits(gtype *g, gtype *p, \
								 float* state1, float* state2, float *unorm, float max_unorm, float param_norm, \
								 const float beta1, const float beta2, const float eps, const float weight_decay, \
								 const int step, const float lr, const float gnorm_scale, const int n) \
	{ name##32bit_g##gbits(g, p, state1, state2, unorm, max_unorm, param_norm, beta1, beta2, eps, weight_decay, step, lr, gnorm_scale, n); } \

	MAKE_CFUNC32(adam, float, 32)
	MAKE_CFUNC32(adam, half, 16)
	MAKE_CFUNC32(momentum, float, 32)
	MAKE_CFUNC32(momentum, half, 16)
	MAKE_CFUNC32(rmsprop, float, 32)
	MAKE_CFUNC32(rmsprop, half, 16)

	#define MAKE_CFUNC8(name, gtype, gbits) \
	void c##name##_static_8bit_g##gbits(gtype* p, gtype* g, unsigned char* state1, unsigned char* state2, \
                float *unorm, float max_unorm, float param_norm, \
                float beta1, float beta2, \
                float eps, int step, float lr,  \
                float* quantiles1, float* quantiles2, \
                float* max1, float* max2, float* new_max1, float* new_max2, \
                float weight_decay, float gnorm_scale, int n) \
  {  \
	    name##_static_8bit_g##gbits(g, p, state1, state2, unorm, max_unorm, param_norm, beta1, beta2, eps, step, lr, \
			                                 quantiles1, quantiles2, max1, max2, new_max1, new_max2, weight_decay, gnorm_scale, n); \
  } \

	MAKE_CFUNC8(adam, float, 32)
	MAKE_CFUNC8(adam, half, 16)
	MAKE_CFUNC8(momentum, float, 32)
	MAKE_CFUNC8(momentum, half, 16)
	MAKE_CFUNC8(rmsprop, float, 32)
	MAKE_CFUNC8(rmsprop, half, 16)

  #define MAKE_CBLOCKWISE8(fname, optim_name, gtype, gbits) \
  void c##fname##_8bit_blockwise_fp##gbits(gtype* p, gtype* g, \
                unsigned char* state1, unsigned char* state2, float beta1, float beta2, float eps, int step, float lr,  \
                float* quantiles1, float* quantiles2, float* absmax1, float* absmax2, float weight_decay, const float gnorm_scale, int n) \
  {	fname##_8bit_blockwise_fp##gbits(p, g, state1, state2, beta1, beta2, eps, step, lr, quantiles1, quantiles2, absmax1, absmax2, weight_decay, gnorm_scale, n); } \

	MAKE_CBLOCKWISE8(adam, ADAM, half, 16)
	MAKE_CBLOCKWISE8(adam, ADAM, float, 32)
	MAKE_CBLOCKWISE8(momentum, MOMENTUM, half, 16)
	MAKE_CBLOCKWISE8(momentum, MOMENTUM, float, 32)
	MAKE_CBLOCKWISE8(rmsprop, RMSPROP, half, 16)
	MAKE_CBLOCKWISE8(rmsprop, RMSPROP, float, 32)


	void cpercentile_clipping_g32(float * g, float *gnorm_vec, int step, const int n){ percentileClipping_g32(g, gnorm_vec, step, n); }
	void cpercentile_clipping_g16(half * g, float *gnorm_vec, int step, const int n){ percentileClipping_g16(g, gnorm_vec, step, n); }
	void cigemm(Context *context, bool transposeA, bool transposeB, int m, int n, int k, void *A, void *B, void *C, int lda, int ldb, int ldc)
	{ gemmex(context, transposeA, transposeB, m, n, k, A, B, C, lda, ldb, ldc); }
	void cbatched_igemm(Context *context, bool transposeA, bool transposeB, int m, int n, int k, void *A, void *B, void *C, int lda, int ldb, int ldc,
			               long strideA, long strideB, long strideC, int batchCount)
	{ strided_gemmex(context, transposeA, transposeB, m, n, k, A, B, C, lda, ldb, ldc, strideA, strideB, strideC, batchCount); }

	Context *get_context(){ return new Context(); }
	ContextCusparse *get_cusparse(){ return new ContextCusparse(); }

	int cigemmlt_turing_32(Context *context, int m, int n, int k, const int8_t *A, const int8_t *B, void *C, float *row_scale, int lda, int ldb, int ldc)
	{ return igemmlt_turing_32((cublasLtHandle_t) context->m_handle, m, n, k, A, B, C, row_scale, lda, ldb, ldc); }

	int cigemmlt_turing_8(Context *context, int m, int n, int k, const int8_t *A, const int8_t *B, void *C, float *row_scale, int lda, int ldb, int ldc)
	{ return igemmlt_turing_8((cublasLtHandle_t) context->m_handle, m, n, k, A, B, C, row_scale, lda, ldb, ldc); }

	int cigemmlt_turing_8_rowscale(Context *context, int m, int n, int k, const int8_t *A, const int8_t *B, void *C, float *row_scale, int lda, int ldb, int ldc)
	{ return igemmlt_turing_8_rowscale((cublasLtHandle_t) context->m_handle, m, n, k, A, B, C, row_scale, lda, ldb, ldc); }

	int cigemmlt_ampere_32(Context *context, int m, int n, int k, const int8_t *A, const int8_t *B, void *C, float *row_scale, int lda, int ldb, int ldc)
	{ return igemmlt_ampere_32((cublasLtHandle_t) context->m_handle, m, n, k, A, B, C, row_scale, lda, ldb, ldc); }

	int cigemmlt_ampere_8_rowscale(Context *context, int m, int n, int k, const int8_t *A, const int8_t *B, void *C, float *row_scale, int lda, int ldb, int ldc)
	{ return igemmlt_ampere_8_rowscale((cublasLtHandle_t) context->m_handle, m, n, k, A, B, C, row_scale, lda, ldb, ldc); }

	int cigemmlt_ampere_8(Context *context, int m, int n, int k, const int8_t *A, const int8_t *B, void *C, float *row_scale, int lda, int ldb, int ldc)
	{ return igemmlt_ampere_8_rowscale((cublasLtHandle_t) context->m_handle, m, n, k, A, B, C, row_scale, lda, ldb, ldc); }

  #define MAKE_FUNC_CTRANSFORM(fbits, fsrc, ftrgt, ftranspose, dtype, src, target, transpose, bits) \
	void ctransform_##fbits##_##fsrc##_to_##ftrgt##_##ftranspose(Context *context, dtype *A, dtype *out, int dim1, int dim2) \
	{ \
		transform_##fbits##_##fsrc##_to_##ftrgt##_##ftranspose((cublasLtHandle_t) context->m_handle, A, out, dim1, dim2); \
	} \


	MAKE_FUNC_CTRANSFORM(8, row, col, n, int8_t, ROW, COL, false, 8)
	MAKE_FUNC_CTRANSFORM(8, row, row, n, int8_t, ROW, ROW, false, 8)
	MAKE_FUNC_CTRANSFORM(8, row, col32, n, int8_t, ROW, COL32, false, 8)
	MAKE_FUNC_CTRANSFORM(32, row, col32, n, int32_t, ROW, COL32, false, 32)
	MAKE_FUNC_CTRANSFORM(8, row, col_turing, n, int8_t, ROW, COL_TURING, false, 8)
	MAKE_FUNC_CTRANSFORM(8, row, col_ampere, n, int8_t, ROW, COL_AMPERE, false, 8)
	MAKE_FUNC_CTRANSFORM(8, col32, row, n, int8_t, COL32, ROW, false, 8)
	MAKE_FUNC_CTRANSFORM(32, col32, row, n, int32_t, COL32, ROW, false, 32)

	void ccutlass_igemm(bool transposeA, bool transposeB, int m, int n, int k, void *A, void *B, void *C, int lda, int ldb, int ldc)
	{ cutlass_igemm(transposeA, transposeB, m, n, k, A, B, C, lda, ldb, ldc); }

	void cdequant_mm_int32_fp16(int *A, float *rowStats, float *colStats, half *out, float* newRowStats, float* newcolStats, int numRows, int numCols)
	{ dequant_mm_int32_fp16(A, rowStats, colStats, out, newRowStats, newcolStats, numRows, numCols); }
	void cget_col_row_stats(half * A, float *rowStats, float *colStats, int *nnz_count_row, float nnz_threshold, int rows, int cols)
	{ getColRowStats(A, rowStats, colStats, nnz_count_row, nnz_threshold, rows, cols); }

  void cdouble_rowcol_quant(half * A, float *rowStats, float *colStats, char *out_col_normed, char *out_row_normed, int *rowidx, int *colidx, half *val, int *nnz_row_ptr, float threshold, int rows, int cols)
	{ doubleRowColQuant(A, rowStats, colStats, out_col_normed, out_row_normed, rowidx, colidx, val, nnz_row_ptr, threshold, rows, cols); }

	void ctransform_row2col32(char * A, char *out, int rows, int cols)
	{ transform_row2col32(A, out, rows, cols); }

	void ctransform_row2col32T(char * A, char *out, int rows, int cols)
	{ transform_row2col32T(A, out, rows, cols); }

	void ctransform_row2turing(char * A, char *out, int rows, int cols)
	{ transform_row2turing(A, out, rows, cols); }

	void ctransform_row2turingT(char * A, char *out, int rows, int cols)
	{ transform_row2turingT(A, out, rows, cols); }

	void ctransform_row2ampere(char * A, char *out, int rows, int cols)
	{ transform_row2ampere(A, out, rows, cols); }

	void ctransform_row2ampereT(char * A, char *out, int rows, int cols)
	{ transform_row2ampereT(A, out, rows, cols); }

	void cspmm_coo(ContextCusparse *context, int *A_rowidx, int *A_colidx, half *A_vals, int A_nnz, int A_rows, int A_cols, int B_cols, int ldb, half *B, int ldc, half* C, bool transposed_B)
  { spmm_coo((cusparseHandle_t) context->m_handle, A_rowidx, A_colidx, A_vals, A_nnz, A_rows, A_cols, B_cols, ldb, B, ldc, C, transposed_B); }

	void cspmm_coo_very_sparse_naive_fp16(int *max_count, int *max_idx, int *offset_rowidx, int *rowidx, int *colidx, half *values, half *B, half *out, float *dequant_stats, int nnz_rows, int nnz, int rowsA, int rowsB, int colsB)
	{ spmm_coo_very_sparse_naive_fp16(max_count, max_idx, offset_rowidx, rowidx, colidx, values, B, out, dequant_stats, nnz_rows, nnz, rowsA, rowsB, colsB); }

	void cspmm_coo_very_sparse_naive_int8(int *max_count, int *max_idx, int *offset_rowidx, int *rowidx, int *colidx, half *values, signed char *B, half *out, float *dequant_stats, int nnz_rows, int nnz, int rowsA, int rowsB, int colsB)
	{ spmm_coo_very_sparse_naive_int8(max_count, max_idx, offset_rowidx, rowidx, colidx, values, B, out, dequant_stats, nnz_rows, nnz, rowsA, rowsB, colsB); }
}


