#pragma once

#include<cstring>
#pragma GCC optimize("O3,unroll-loops")
#pragma GCC target("avx2")

constexpr int BLOCK = 64;

void multiply(const int *a, const int *b, int *c, int n)
{
    std::memset(c, 0, sizeof(int) * n * n);

    for (int ii = 0; ii < n; ii += BLOCK) {
        const int i_end = ii + BLOCK < n ? ii + BLOCK : n;
        for (int kk = 0; kk < n; kk += BLOCK) {
            const int k_end = kk + BLOCK < n ? kk + BLOCK : n;
            for (int jj = 0; jj < n; jj += BLOCK) {
                const int j_end = jj + BLOCK < n ? jj + BLOCK : n;
                for (int i = ii; i < i_end; ++i) {
                    int *__restrict__ pc = c + i * n;
                    const int *__restrict__ pa = a + i * n;
                    for (int k = kk; k < k_end; ++k) {
                        const int aik = pa[k];
                        const int *__restrict__ pb = b + k * n;
                        for (int j = jj; j < j_end; ++j) {
                            pc[j] += aik * pb[j];
                        }
                    }
                }
            }
        }
    }
}
