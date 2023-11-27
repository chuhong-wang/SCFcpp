#include "scf.h" 
#include <sstream> 
#include <iostream> 
#include "Eigen/Dense"
#include "Eigen/Eigenvalues"
#include "Eigen/Core"

#define INDEX(i,j) (i>j) ? (i+1)*i/2+j : (j+1)*j/2+i 

// constructor of Scf initializes its molecule 
Scf::Scf(std::string geom_file, int q, int n_ao):mol(geom_file, q, n_ao) {
    mol.read_1e_integral("int1e_overlap"); 
    mol.read_1e_integral("int1e_kinetic"); 
    mol.read_1e_integral("int1e_nuc"); 
    mol.read_2e_integral("data/eri.dat"); 
} 

Molecule Scf::get_molecule() {return mol; }

Matrix Scf::vector_to_Matrix(const std::vector<double> &vec){
    int nao = mol.nao_; 
    Matrix mat_out(nao, nao);
    for (auto i = 0; i!=nao; ++i){
        for (auto j = 0; j!=nao; ++j){
            mat_out(i,j) = mol.one_e_overlap_en()[i*nao+j]; 
        }
    }
    return mat_out; 
}

/**
 * diagonalization of a matrix
 * return: eigenvectors and eigenvalues  
*/
std::pair<Matrix, Matrix> Scf::eigenVec_eigenVal(const Matrix &mat){
    Eigen::SelfAdjointEigenSolver<Matrix> solver(mat); 
    Matrix evals = solver.eigenvalues(); 
    Matrix evecs = solver.eigenvectors(); 
    return {evecs, evals}; 
}

/**
 * diagonalization of overlap integral S 
 * SL_s = L_sΛ_s 
 * where L_s is the eigenvector as columns, 
 * and Λ_s is the eigenvalues on the diagonal of the matrix 
 * S^(-1/2) = L_s * Λ_s^(-1/2) * L_s.T
*/
Matrix Scf::orthogonalization_matrix(const std::vector<double> &vec){
    Matrix mat_out = vector_to_Matrix(vec); 
    auto eigen_out = eigenVec_eigenVal(mat_out);  
    Matrix evecs= eigen_out.first;
    Matrix evals =  eigen_out.second; 

    Matrix diag_evals(mol.nao_, mol.nao_); 
    for (auto i = 0;i!=mol.nao_; ++i) {
        diag_evals(i,i) = evals(i); 
    }
    Matrix matrix_sqrt = diag_evals.llt().matrixL(); // this is equivalent to squre root  
    diag_evals = matrix_sqrt.inverse(); 

    Matrix S_inv_sqrt = evecs * diag_evals * evecs.transpose(); 
    return S_inv_sqrt; 
}

Matrix Scf::H_core(){
    Matrix tmp(mol.nao_, mol.nao_); 
    for (auto i = 0; i!=mol.nao_; ++i){
        for (auto j = 0; j!=mol.nao_; ++j){
            tmp(i,j) = mol.one_e_kinetic_en()[i*mol.nao_+j] + mol.one_e_nuclearAttraction_en()[i*mol.nao_+j]; 
        }
    }
    return tmp; 
}

Matrix Scf::orthogonalize(const Matrix &mat, const Matrix &orthogonalization_matrix_){
    Matrix mat_out = orthogonalization_matrix_.transpose() * mat * orthogonalization_matrix_; 
    return mat_out; 
}

void Scf::update_density(
    const Matrix& eigenVec_AObasis,
    int occupied_ao,
    Matrix &orig_density)
    {
        Matrix C0_occAO = eigenVec_AObasis.block(0, 0, mol.nao_, occupied_ao);
        orig_density = C0_occAO * C0_occAO.transpose();
}

void Scf::update_Fork_matrix(const Matrix &h_core_, const Matrix &D, Matrix &orig_Fork){
    for (int i = 0; i!=mol.nao_; ++i){
        for (int j = 0; j!=mol.nao_; ++j){
            orig_Fork(i,j) = h_core_(i,j); 
            for (int k=0; k!=mol.nao_; ++k){
                for (int l=0; l!=mol.nao_; ++l){
                    int ij = INDEX(i,j); 
                    int kl = INDEX(k,l); 
                    int ijkl = INDEX(ij, kl);                        
                    int ik = INDEX(i, k); 
                    int jl = INDEX(j, l); 
                    int ikjl = INDEX(ik, jl); 
                    orig_Fork(i,j) += D(k,l) * (2.0*mol.eri_[ijkl] - mol.eri_[ikjl]); 
                    
                }
            }
        }
    }
}

double Scf::SCF_energy(const Matrix &density, const Matrix &h_core_, const Matrix &F_uv){
    return (density.array()*(h_core_ + F_uv).array()).sum(); 
}