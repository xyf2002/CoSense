/*
	Authored 2018. Vlad Mihai Mandric.

	Based on skeleton implementation of Eigen interface by Phillip Stanley-Marbell.

	All rights reserved.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions
	are met:

	*	Redistributions of source code must retain the above
		copyright notice, this list of conditions and the following
		disclaimer.

	*	Redistributions in binary form must reproduce the above
		copyright notice, this list of conditions and the following
		disclaimer in the documentation and/or other materials
		provided with the distribution.

	*	Neither the name of the author nor the names of its
		contributors may be used to endorse or promote products
		derived from this software without specific prior written
		permission.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
	"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
	LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
	FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
	COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
	INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
	BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
	LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
	CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
	LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
	ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
	POSSIBILITY OF SUCH DAMAGE.
*/

//#include "newton-eigenLibraryInterface.h"

#include <Eigen/Eigen>
#include <iostream>
using namespace std;
using namespace Eigen;

extern "C"
{
	int
	factorial(int N)
	{
		int product = 1;
		for(int i = 2; i <= N; i++)
		{
			product *= i;
		}
		
		return product;
	}

	void
	computeRREF(MatrixXf & m, int N, int M, int indices[]) //	Computes the row reduced echelon form(RREF)
	{
		int i = 0, j = 0, r = 0;
		
		while (i < N && j < M)
		{
			/*
			 *	Step 1
			 */
			int p = 1;
			int k = j;

			while (k < N)
			{
				if (m(i, k) == 0)
				{
					do
					{
						m.row(i).swap(m.row(i+p));
						p++;
					} while(m(i, k) == 0);
				}

				if (m(i, k) == 0)
				{
					indices[r] = k;
					r++;
					k++;
				}
				else
				{
					break;
				}
			}

			/*
			 *	Step 2
			 */
			m.array().row(i) /= m(i, j);

			/*
			 *	Step 3
			 */
			for (int q = 0; q < N; q++)
			{
				if (q != i)
				{
					m.array().row(q) -= m.array().row(i) * m(q, j) / m(i, j); 
				}
			}
			
			/*
			 *	Step 4
			 */
			i++;
			j++;
		}

		return;
	}

	void
	computePiGroup(MatrixXf m, int N, int M, int rank, int x[], MatrixXf ker[], int element)
	{
		for (int i = 1; i <= rank; i++)
		{
			if(x[i] > i)
			{
				m.col(i-1).swap(m.col(x[i]-1));
			}
		}

		int indices[M - rank];
		computeRREF(m, N, M, indices);
		
		MatrixXf nonPivot(rank, M - rank);
		for (int i = 0; i < M - rank; i++)
		{
			nonPivot.col(i) = m.block(0, indices[i], rank, 1); 
		}
		nonPivot.array() = nonPivot.array() * -1;

		MatrixXf I = MatrixXf::Identity(M - rank, M - rank);
		
		int r = 0, p = 0;
		for (int i = 0; i < M; i++)
		{
			if (i == indices[r])
			{
				ker[element].row(i) = I.row(r);
				r++;
			}
			else
			{
				ker[element].row(i) = nonPivot.row(p);
				p++; 
			}
		}

		return;
	}

	void
	generateAllPiGroups(MatrixXf mat, int N, int M,int rank, int x[], int k, MatrixXf ker[])
	{
		int element = 0;
		if (k == rank + 1)
		{
			computePiGroup(mat, N, M, rank, x, ker, element);
			element ++;
		}
		else
		{
			for (int i = x[k-1] + 1; i <= M -rank + k; i++)
			{
				x[k] = i;
				generateAllPiGroups(mat, N, M, rank, x, k + 1, ker);
			}
		}
	}

	void 
	getPiGroups(float *m, int N, int M)									// N = #rows	M = #columns
	{
		Map<MatrixXf> tmp (m, M, N);
		MatrixXf mat = tmp.transpose();
		
		int rank = mat.fullPivLu().rank();
		cout << "The rank of the matrix is " << rank << endl;
		
		int x[rank+1];
		x[0] = 0;
		int k = 1;

		int numberCircuitSets = factorial(M) / (factorial(k) * factorial(M - rank));
		MatrixXf ker[numberCircuitSets];

		generateAllPiGroups(mat, N, M, rank, x, k, ker);

		cout << endl << endl << ker[0] << endl << endl << ker[1] << endl;
	
		return;
	}
} /* extern "C" */
