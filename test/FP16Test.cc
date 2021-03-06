/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 * All rights reserved.
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */
#include <random>

#include <gtest/gtest.h>

#include "fbgemm/FbgemmFP16.h"
#include "src/RefImplementations.h"
#include "bench/BenchUtils.h"
#include "TestUtils.h"

#ifdef USE_IACA
#include "iacaMarks.h"
#endif

using namespace std;
using namespace fbgemm2;

namespace {
  // The template parameter is transpose of A and B
  class FBGemmFP16Test :
    public testing::TestWithParam<pair<matrix_op_t, matrix_op_t>> {};
}; // namespace

INSTANTIATE_TEST_CASE_P(
    InstantiationName,
    FBGemmFP16Test,
    ::testing::Values(
      pair<matrix_op_t, matrix_op_t>(
          matrix_op_t::NoTranspose, matrix_op_t::NoTranspose),
      pair<matrix_op_t, matrix_op_t>(
          matrix_op_t::NoTranspose, matrix_op_t::Transpose)/*,
      pair<matrix_op_t, matrix_op_t>(
          matrix_op_t::Transpose, matrix_op_t::NoTranspose),
      pair<matrix_op_t, matrix_op_t>(
          matrix_op_t::Transpose, matrix_op_t::Transpose)*/));

TEST_P(FBGemmFP16Test, Test) {
  vector<vector<int> > shapes;
  random_device r;
  default_random_engine generator(r());
  uniform_int_distribution<int> dm(1,100);
  uniform_int_distribution<int> dnk(1,1024);
  for (int i = 0; i < 10; i++) {
    int m = dm(generator);
    int n = dnk(generator);
    int k = dnk(generator);
    shapes.push_back({m, n, k});
    if (m > 10) {
      shapes.push_back({(m / 10) * 10, n, k});
    }
  }

  float alpha = 1.f, beta = 0.f;
  matrix_op_t atrans, btrans;
  tie(atrans, btrans) = GetParam();

  for (auto s : shapes) {
    int m = s[0];
    int n = s[1];
    int k = s[2];

    cerr << "m = " << m << " n = " << n << " k = " << k;
    if (atrans == matrix_op_t::Transpose) {
      cerr << " A_transposed";
    }
    if (btrans == matrix_op_t::Transpose) {
      cerr << " B_transposed";
    }
    cerr << endl;

    aligned_vector<float> A(m * k, 0.f);
    aligned_vector<float> B(k * n, 0.f);
    aligned_vector<float> C(m * n, 0.f);

    // initialize with small numbers
    randFill(A, 0, 4);
    randFill(B, 0, 4);
    randFill(C, 0, 4);

    aligned_vector<float> A_ref, B_ref, C_ref;
    A_ref = A;
    B_ref = B;
    C_ref = C;

    if (atrans == matrix_op_t::Transpose) {
      transpose_matrix(A_ref.data(), k, m);
    }
    if (btrans == matrix_op_t::Transpose) {
      transpose_matrix(B_ref.data(), n, k);
    }

    // Gold via reference sgemm
    matmul_fp_ref(
        m,
        n,
        k,
        k,
        n,
        n,
        A_ref.data(),
        B_ref.data(),
        C_ref.data());

    // fbgemm fp16
    PackedGemmMatrixFP16 Bp(btrans, k, n, alpha, B.data());
    cblas_gemm_compute(atrans, m, A.data(), Bp, beta, C.data());

    // correctness check
    for (int i = 0; i < m; ++i) {
      for (int j = 0; j < n; ++j) {
        float expected = C_ref[i * n + j];
        float actual = C[i * n + j];
        EXPECT_EQ(expected, actual) <<
          "GEMM results differ at (" << i << ", " << j <<
          "). ref " << expected << " FBGemm " << actual;
      }
    }
  }
}
