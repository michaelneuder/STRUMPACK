/*
 * STRUMPACK -- STRUctured Matrices PACKage, Copyright (c) 2014, The
 * Regents of the University of California, through Lawrence Berkeley
 * National Laboratory (subject to receipt of any required approvals
 * from the U.S. Dept. of Energy).  All rights reserved.
 *
 * If you have questions about your rights to use or distribute this
 * software, please contact Berkeley Lab's Technology Transfer
 * Department at TTD@lbl.gov.
 *
 * NOTICE. This software is owned by the U.S. Department of Energy. As
 * such, the U.S. Government has been granted for itself and others
 * acting on its behalf a paid-up, nonexclusive, irrevocable,
 * worldwide license in the Software to reproduce, prepare derivative
 * works, and perform publicly and display publicly.  Beginning five
 * (5) years after the date permission to assert copyright is obtained
 * from the U.S. Department of Energy, and subject to any subsequent
 * five (5) year renewals, the U.S. Government is granted for itself
 * and others acting on its behalf a paid-up, nonexclusive,
 * irrevocable, worldwide license in the Software to reproduce,
 * prepare derivative works, distribute copies to the public, perform
 * publicly and display publicly, and to permit others to do so.
 *
 * Developers: Pieter Ghysels, Francois-Henry Rouet, Xiaoye S. Li.
 *             (Lawrence Berkeley National Lab, Computational Research
 *             Division).
 *
 */
/*! \file BLRMatrixMPI.hpp
 * \brief Distributed memory block-low rank matrix format.
 */
#ifndef BLR_MATRIX_MPI_HPP
#define BLR_MATRIX_MPI_HPP

#include "dense/DistributedMatrix.hpp"
#include "BLRMatrix.hpp"
#include "BLRTile.hpp"

namespace strumpack {
  namespace BLR {

    class ProcessorGrid2D {
    public:
      ProcessorGrid2D(const MPIComm& comm);
      ProcessorGrid2D(const MPIComm& comm, int P);

      const MPIComm& Comm() const { return comm_; }
      int nprows() const { return nprows_; }
      int npcols() const { return npcols_; }
      int prow() const { return prow_; }
      int pcol() const { return pcol_; }
      int rank() const { return Comm().rank(); }
      int npactives() const { return nprows()*npcols(); }
      bool active() const { return active_; }

      const MPIComm& row_comm() const { return rowcomm_; }
      const MPIComm& col_comm() const { return colcomm_; }

      bool is_local_row(int i) const { return i % nprows_ == prow_; }
      bool is_local_col(int i) const { return i % npcols_ == pcol_; }
      bool is_local(int i, int j) const
      { return is_local_row(i) & is_local_col(j); }

      int rg2p(int i) const { return i % nprows(); }
      int cg2p(int j) const { return j % npcols(); }
      int g2p(int i, int j) const { return rg2p(i) + cg2p(j) * nprows(); }

      void print() const {
        if (comm_.is_root())
          std::cout << "# ProcessorGrid2D: "
                    << "[" << nprows() << " x " << npcols() << "]"
                    << std::endl;
      }

    private:
      bool active_ = false;
      int prow_ = -1, pcol_ = -1;
      int nprows_ = 0, npcols_ = 0;
      MPIComm comm_, rowcomm_, colcomm_;
    };


    template<typename scalar_t> class BLRMatrixMPI {
      using DenseM_t = DenseMatrix<scalar_t>;
      using DenseMW_t = DenseMatrixWrapper<scalar_t>;
      using DistM_t = DistributedMatrix<scalar_t>;
      using DistMW_t = DistributedMatrixWrapper<scalar_t>;
      using BLRMPI_t = BLRMatrixMPI<scalar_t>;
      using vec_t = std::vector<std::size_t>;
      using adm_t = DenseMatrix<bool>;
      using Opts_t = BLROptions<scalar_t>;

    public:
      BLRMatrixMPI();
      BLRMatrixMPI(const ProcessorGrid2D& grid,
                   const vec_t& Rt, const vec_t& Ct);

      std::size_t rows() const { return rows_; }
      std::size_t cols() const { return cols_; }

      std::size_t memory() const;
      std::size_t nonzeros() const;
      std::size_t rank() const;
      std::size_t total_memory() const;
      std::size_t total_nonzeros() const;
      std::size_t max_rank() const;

      const MPIComm& Comm() const { return grid_->Comm(); }

      const ProcessorGrid2D* grid() const { return grid_; }

      bool active() const { return grid_->active(); }

      void fill(scalar_t v);

      std::vector<int> factor(const Opts_t& opts);
      std::vector<int> factor(const adm_t& adm, const Opts_t& opts);

      void laswp(const std::vector<int>& piv, bool fwd);

      static
      std::vector<int> partial_factor(BLRMPI_t& A11, BLRMPI_t& A12,
                                      BLRMPI_t& A21, BLRMPI_t& A22,
                                      const adm_t& adm, const Opts_t& opts);

      void compress(const Opts_t& opts);

      static
      BLRMPI_t from_ScaLAPACK(const DistM_t& A, const ProcessorGrid2D& g,
                              const Opts_t& opts);
      static
      BLRMPI_t from_ScaLAPACK(const DistM_t& A, const ProcessorGrid2D& g,
                              const vec_t& Rt, const vec_t& Ct);
      DistM_t to_ScaLAPACK(const BLACSGrid* g) const;
      void to_ScaLAPACK(DistM_t& A) const;

      void print(const std::string& name);

      std::size_t rowblocks() const { return brows_; }
      std::size_t colblocks() const { return bcols_; }
      std::size_t rowblockslocal() const { return lbrows_; }
      std::size_t colblockslocal() const { return lbcols_; }
      std::size_t tilerows(std::size_t i) const { return roff_[i+1] - roff_[i]; }
      std::size_t tilecols(std::size_t j) const { return coff_[j+1] - coff_[j]; }
      std::size_t tileroff(std::size_t i) const { return roff_[i]; }
      std::size_t tilecoff(std::size_t j) const { return coff_[j]; }

      int rg2p(std::size_t i) const;
      int cg2p(std::size_t j) const;
      std::size_t rl2g(std::size_t i) const;
      std::size_t cl2g(std::size_t j) const;
      std::size_t rg2t(std::size_t i) const;
      std::size_t cg2t(std::size_t j) const;

      std::size_t lrows() const { return lrows_; }
      std::size_t lcols() const { return lcols_; }

      /**
       * Direct access to element with local indexing, only for dense
       * tiles, for instance before the factorization/compression.
       */
      const scalar_t& operator()(std::size_t i, std::size_t j) const;
      scalar_t& operator()(std::size_t i, std::size_t j);

      /**
       * Same as operator()(std::size_t i, std::size_t j), but with
       * global indexing. This assumes the global element is stored
       * locally, otherwise behaviour is undefined.
       */
      const scalar_t& global(std::size_t i, std::size_t j) const;
      scalar_t& global(std::size_t i, std::size_t j);

    private:
      std::size_t rows_ = 0, cols_ = 0, lrows_ = 0, lcols_ = 0;
      std::size_t brows_ = 0, bcols_ = 0, lbrows_ = 0, lbcols_ = 0;
      vec_t roff_, coff_;
      vec_t rl2t_, cl2t_, rl2l_, cl2l_, rl2g_, cl2g_;
      std::vector<std::unique_ptr<BLRTile<scalar_t>>> blocks_;
      const ProcessorGrid2D* grid_ = nullptr;

      std::size_t tilerg2l(std::size_t i) const {
        assert(int(i % grid_->nprows()) == grid_->prow());
        return i / grid_->nprows();
      }
      std::size_t tilecg2l(std::size_t j) const {
        assert(int(j % grid_->npcols()) == grid_->pcol());
        return j / grid_->npcols();
      }

      BLRTile<scalar_t>& tile(std::size_t i, std::size_t j) {
        return ltile(tilerg2l(i), tilecg2l(j));
      }
      const BLRTile<scalar_t>& tile(std::size_t i, std::size_t j) const {
        return ltile(tilerg2l(i), tilecg2l(j));
      }
      DenseTile<scalar_t>& tile_dense(std::size_t i, std::size_t j) {
        return ltile_dense(tilerg2l(i), tilecg2l(j));
      }
      const DenseTile<scalar_t>& tile_dense(std::size_t i, std::size_t j) const {
        return ltile_dense(tilerg2l(i), tilecg2l(j));
      }

      BLRTile<scalar_t>& ltile(std::size_t i, std::size_t j) {
        assert(i < rowblockslocal() && j < colblockslocal());
        return *blocks_[i+j*rowblockslocal()].get();
      }
      const BLRTile<scalar_t>& ltile(std::size_t i, std::size_t j) const {
        assert(i < rowblockslocal() && j < colblockslocal());
        return *blocks_[i+j*rowblockslocal()].get();
      }

      DenseTile<scalar_t>& ltile_dense(std::size_t i, std::size_t j) {
        assert(i < rowblockslocal() && j < colblockslocal());
        assert(dynamic_cast<DenseTile<scalar_t>*>
               (blocks_[i+j*rowblockslocal()].get()));
        return *static_cast<DenseTile<scalar_t>*>
                   (blocks_[i+j*rowblockslocal()].get());
      }
      const DenseTile<scalar_t>& ltile_dense(std::size_t i, std::size_t j) const {
        assert(i < rowblockslocal() && j < colblockslocal());
        assert(dynamic_cast<const DenseTile<scalar_t>*>
               (blocks_[i+j*rowblockslocal()].get()));
        return *static_cast<const DenseTile<scalar_t>*>
                   (blocks_[i+j*rowblockslocal()].get());
      }

      std::unique_ptr<BLRTile<scalar_t>>&
      block(std::size_t i, std::size_t j) {
        assert(i < rowblocks() && j < colblocks());
        return blocks_[tilerg2l(i)+tilecg2l(j)*rowblockslocal()];
      }
      const std::unique_ptr<BLRTile<scalar_t>>&
      block(std::size_t i, std::size_t j) const {
        assert(i < rowblocks() && j < colblocks());
        return blocks_[tilerg2l(i)+tilecg2l(j)*rowblockslocal()];
      }

      std::unique_ptr<BLRTile<scalar_t>>&
      lblock(std::size_t i, std::size_t j) {
        assert(i < rowblockslocal() && j < colblockslocal());
        return blocks_[i+j*rowblockslocal()];
      }
      const std::unique_ptr<BLRTile<scalar_t>>&
      lblock(std::size_t i, std::size_t j) const {
        assert(i < rowblockslocal() && j < colblockslocal());
        return blocks_[i+j*rowblockslocal()];
      }

      void compress_tile(std::size_t i, std::size_t j, const Opts_t& opts);

      DenseTile<scalar_t>
      bcast_dense_tile_along_col(std::size_t i, std::size_t j) const;
      DenseTile<scalar_t>
      bcast_dense_tile_along_row(std::size_t i, std::size_t j) const;

      std::vector<std::unique_ptr<BLRTile<scalar_t>>>
      bcast_row_of_tiles_along_cols
      (std::size_t i, std::size_t j0, std::size_t j1) const;
      std::vector<std::unique_ptr<BLRTile<scalar_t>>>
      bcast_col_of_tiles_along_rows
      (std::size_t i0, std::size_t i1, std::size_t j) const;


      template<typename T> friend void
      trsv(UpLo ul, Trans ta, Diag d, const BLRMatrixMPI<T>& a,
           BLRMatrixMPI<T>& b);
      template<typename T> friend void
      gemv(Trans ta, T alpha, const BLRMatrixMPI<T>& a,
           const BLRMatrixMPI<T>& x, T beta, BLRMatrixMPI<T>& y);
      template<typename T> friend void
      trsm(Side s, UpLo ul, Trans ta, Diag d, T alpha,
           const BLRMatrixMPI<T>& a, BLRMatrixMPI<T>& b);
      template<typename T> friend void
      gemm(Trans ta, Trans tb, T alpha, const BLRMatrixMPI<T>& a,
           const BLRMatrixMPI<T>& b, T beta, BLRMatrixMPI<T>& c);
    };


    template<typename scalar_t> void
    trsv(UpLo ul, Trans ta, Diag d, const BLRMatrixMPI<scalar_t>& a,
         BLRMatrixMPI<scalar_t>& b);
    template<typename scalar_t> void
    gemv(Trans ta, scalar_t alpha, const BLRMatrixMPI<scalar_t>& a,
         const BLRMatrixMPI<scalar_t>& x, scalar_t beta,
         BLRMatrixMPI<scalar_t>& y);

    template<typename scalar_t> void
    trsm(Side s, UpLo ul, Trans ta, Diag d,
         scalar_t alpha, const BLRMatrixMPI<scalar_t>& a,
         BLRMatrixMPI<scalar_t>& b);
    template<typename scalar_t> void
    gemm(Trans ta, Trans tb, scalar_t alpha, const BLRMatrixMPI<scalar_t>& a,
         const BLRMatrixMPI<scalar_t>& b, scalar_t beta,
         BLRMatrixMPI<scalar_t>& c);

  } // end namespace BLR
} // end namespace strumpack

#endif // BLR_MATRIX_MPI_HPP
