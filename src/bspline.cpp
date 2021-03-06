/*
 * This file is part of the SPLINTER library.
 * Copyright (C) 2012 Bjarne Grimstad (bjarne.grimstad@gmail.com).
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "bspline.h"
#include "bsplinebasis.h"
#include "mykroneckerproduct.h"
#include "unsupported/Eigen/KroneckerProduct"
#include <linearsolvers.h>
#include <serializer.h>
#include <iostream>
#include <utilities.h>

namespace std
{

template <typename T>

std::string to_string(T value)
{
  std::ostringstream os ;
  os << value ;
  return os.str() ;
}

}

namespace SPLINTER
{

BSpline::BSpline()
    : Approximant(1)
{}

BSpline::BSpline(unsigned int numVariables)
    : Approximant(numVariables)
{}

/*
 * Constructors for multivariate B-spline using explicit data
 */
BSpline::BSpline(std::vector< std::vector<double> > knotVectors, std::vector<unsigned int> basisDegrees)
    : Approximant(knotVectors.size()),
      basis(BSplineBasis(knotVectors, basisDegrees)),
      coefficients(DenseMatrix::Ones(1, basis.getNumBasisFunctions()))
{
    computeKnotAverages();

    checkControlPoints();
}

BSpline::BSpline(std::vector<double> coefficients, std::vector< std::vector<double> > knotVectors, std::vector<unsigned int> basisDegrees)
    : BSpline(vectorToDenseVector(coefficients), knotVectors, basisDegrees)
{
}

BSpline::BSpline(DenseMatrix coefficients, std::vector< std::vector<double> > knotVectors, std::vector<unsigned int> basisDegrees)
    : Approximant(knotVectors.size()),
      basis(BSplineBasis(knotVectors, basisDegrees)),
      coefficients(coefficients)
{
    computeKnotAverages();

    checkControlPoints();
}

/*
 * Construct from saved data
 */
BSpline::BSpline(const char *fileName)
    : BSpline(std::string(fileName))
{
}

BSpline::BSpline(const std::string fileName)
    : Approximant(1)
{
    load(fileName);
}

double BSpline::eval(DenseVector x) const
{
	if (!pointInDomain(x))
	{
		throw Exception("BSpline::eval: Evaluation at point outside domain.");
	}

	SparseVector tensorvalues = basis.eval(x);
	DenseVector y = coefficients*tensorvalues;
	return y(0);
}

/*
 * Returns the Jacobian evaluated at x.
 * The Jacobian is an 1 x n matrix,
 * where n is the dimension of x.
 */
DenseMatrix BSpline::evalJacobian(DenseVector x) const
{
    if (!pointInDomain(x))
    {
        throw Exception("BSpline::evalJacobian: Evaluation at point outside domain.");
    }

    //SparseMatrix Bi = basis.evalBasisJacobian(x);       // Sparse Jacobian implementation
    //SparseMatrix Bi = basis.evalBasisJacobian2(x);  // Sparse Jacobian implementation
    DenseMatrix Bi = basis.evalBasisJacobianOld(x);  // Old Jacobian implementation

    return coefficients*Bi;
}

/*
 * Returns the Hessian evaluated at x.
 * The Hessian is an n x n matrix,
 * where n is the dimension of x.
 */
DenseMatrix BSpline::evalHessian(DenseVector x) const
{
    if (!pointInDomain(x))
    {
        throw Exception("BSpline::evalHessian: Evaluation at point outside domain.");
    }

    DenseMatrix H;
    H.setZero(1,1);
    DenseMatrix identity = DenseMatrix::Identity(numVariables,numVariables);
    DenseMatrix caug;
    caug = kroneckerProduct(identity, coefficients);
    DenseMatrix DB = basis.evalBasisHessian(x);
    H = caug*DB;

    // Fill in upper triangular of Hessian
    for (size_t i = 0; i < numVariables; ++i)
    {
        for (size_t j = i+1; j < numVariables; ++j)
        {
            H(i,j) = H(j,i);
        }
    }
    return H;
}

std::vector<unsigned int> BSpline::getNumBasisFunctions() const
{
    std::vector<unsigned int> ret;
    for (unsigned int i = 0; i < numVariables; i++)
        ret.push_back(basis.getNumBasisFunctions(i));
    return ret;
}

std::vector< std::vector<double> > BSpline::getKnotVectors() const
{
    return basis.getKnotVectors();
}

std::vector<unsigned int> BSpline::getBasisDegrees() const
{
    return basis.getBasisDegrees();
}

std::vector<double> BSpline::getDomainUpperBound() const
{
    return basis.getSupportUpperBound();
}

std::vector<double> BSpline::getDomainLowerBound() const
{
    return basis.getSupportLowerBound();
}

DenseMatrix BSpline::getControlPoints() const
{
    int nc = coefficients.cols();
    DenseMatrix controlPoints(numVariables + 1, nc);

    controlPoints.block(0, 0, numVariables, nc) = knotaverages;
    controlPoints.block(numVariables, 0, 1, nc) = coefficients;

    return controlPoints;
}

void BSpline::setCoefficients(const DenseMatrix &coefficients)
{
    this->coefficients = coefficients;
    checkControlPoints();
}

void BSpline::setControlPoints(const DenseMatrix &controlPoints)
{
    assert(controlPoints.rows() == numVariables + 1);
    int nc = controlPoints.cols();

    knotaverages = controlPoints.block(0, 0, numVariables, nc);
    coefficients = controlPoints.block(numVariables, 0, 1, nc);

    checkControlPoints();
}

// TODO: change return type to bool and name to checkDataConsistency
void BSpline::checkControlPoints() const
{
    if (coefficients.cols() != knotaverages.cols())
        throw Exception("BSpline::checkControlPoints: Inconsistent size of coefficients and knot averages matrices.");
    if (knotaverages.rows() != numVariables)
        throw Exception("BSpline::checkControlPoints: Inconsistent size of knot averages matrix.");
    if (coefficients.rows() != 1)
        throw Exception("BSpline::checkControlPoints: Coefficients matrix does not have one row.");
}

bool BSpline::pointInDomain(DenseVector x) const
{
    return basis.insideSupport(x);
}

void BSpline::reduceDomain(std::vector<double> lb, std::vector<double> ub, bool doRegularizeKnotVectors)
{
    if (lb.size() != numVariables || ub.size() != numVariables)
        throw Exception("BSpline::reduceDomain: Inconsistent vector sizes!");

    std::vector<double> sl = basis.getSupportLowerBound();
    std::vector<double> su = basis.getSupportUpperBound();

    for (unsigned int dim = 0; dim < numVariables; dim++)
    {
        // Check if new domain is empty
        if (ub.at(dim) <= lb.at(dim) || lb.at(dim) >= su.at(dim) || ub.at(dim) <= sl.at(dim))
            throw Exception("BSpline::reduceDomain: Cannot reduce B-spline domain to empty set!");

        // Check if new domain is a strict subset
        if (su.at(dim) < ub.at(dim) || sl.at(dim) > lb.at(dim))
            throw Exception("BSpline::reduceDomain: Cannot expand B-spline domain!");

        // Tightest possible
        sl.at(dim) = lb.at(dim);
        su.at(dim) = ub.at(dim);
    }

    if (doRegularizeKnotVectors)
    {
        regularizeKnotVectors(sl, su);
    }

    // Remove knots and control points that are unsupported with the new bounds
    if (!removeUnsupportedBasisFunctions(sl, su))
    {
        throw Exception("BSpline::reduceDomain: Failed to remove unsupported basis functions!");
    }
}

void BSpline::globalKnotRefinement()
{
    // Compute knot insertion matrix
    SparseMatrix A = basis.refineKnots();

    // Update control points
    assert(A.cols() == coefficients.cols());
    coefficients = coefficients*A.transpose();
    knotaverages = knotaverages*A.transpose();
}

void BSpline::localKnotRefinement(DenseVector x)
{
    // Compute knot insertion matrix
    SparseMatrix A = basis.refineKnotsLocally(x);

    // Update control points
    assert(A.cols() == coefficients.cols());
    coefficients = coefficients*A.transpose();
    knotaverages = knotaverages*A.transpose();
}

void BSpline::decomposeToBezierForm()
{
    // Compute knot insertion matrix
    SparseMatrix A = basis.decomposeToBezierForm();

    // Update control points
    assert(A.cols() == coefficients.cols());
    coefficients = coefficients*A.transpose();
    knotaverages = knotaverages*A.transpose();
}

// Computes knot averages: assumes that basis is initialized!
void BSpline::computeKnotAverages()
{
    // Calculate knot averages for each knot vector
    std::vector< DenseVector > knotAverages;
    for (unsigned int i = 0; i < numVariables; i++)
    {
        std::vector<double> knots = basis.getKnotVector(i);
        DenseVector mu = DenseVector::Zero(basis.getNumBasisFunctions(i));

        for (unsigned int j = 0; j < basis.getNumBasisFunctions(i); j++)
        {
            double knotAvg = 0;
            for (unsigned int k = j+1; k <= j+basis.getBasisDegree(i); k++)
            {
                knotAvg += knots.at(k);
            }
            mu(j) = knotAvg/basis.getBasisDegree(i);
        }
        knotAverages.push_back(mu);
    }

    // Calculate vectors of ones (with same length as corresponding knot average vector)
    std::vector<DenseVector> knotOnes;
    for (unsigned int i = 0; i < numVariables; i++)
        knotOnes.push_back(DenseVector::Ones(knotAverages.at(i).rows()));

    // Fill knot average matrix one row at the time
    // NOTE: Must have same pattern as samples in DataTable
    // NOTE: Could use DataTable to achieve the same pattern
    knotaverages.resize(numVariables, basis.getNumBasisFunctions());

    for (unsigned int i = 0; i < numVariables; i++)
    {
        DenseMatrix mu_ext(1,1); mu_ext(0,0) = 1;
        for (unsigned int j = 0; j < numVariables; j++)
        {
            DenseMatrix temp = mu_ext;
            if (i == j)
                mu_ext = Eigen::kroneckerProduct(temp, knotAverages.at(j));
            else
                mu_ext = Eigen::kroneckerProduct(temp, knotOnes.at(j));
        }
        assert(mu_ext.rows() == basis.getNumBasisFunctions());
        knotaverages.block(i,0,1,basis.getNumBasisFunctions()) = mu_ext.transpose();
    }

    assert(knotaverages.rows() == numVariables && knotaverages.cols() == basis.getNumBasisFunctions());
}

void BSpline::insertKnots(double tau, unsigned int dim, unsigned int multiplicity)
{
    // Insert knots and compute knot insertion matrix
    SparseMatrix A = basis.insertKnots(tau, dim, multiplicity);

    // Update control points
    assert(A.cols() == coefficients.cols());
    coefficients = coefficients*A.transpose();
    knotaverages = knotaverages*A.transpose();
}

void BSpline::regularizeKnotVectors(std::vector<double> &lb, std::vector<double> &ub)
{
    // Add and remove controlpoints and knots to make the b-spline p-regular with support [lb, ub]
    if (!(lb.size() == numVariables && ub.size() == numVariables))
        throw Exception("BSpline::regularizeKnotVectors: Inconsistent vector sizes.");

    for (unsigned int dim = 0; dim < numVariables; dim++)
    {
        unsigned int multiplicityTarget = basis.getBasisDegree(dim) + 1;

        // Inserting many knots at the time (to save number of B-spline coefficient calculations)
        // NOTE: This method generates knot insertion matrices with more nonzero elements than
        // the method that inserts one knot at the time. This causes the preallocation of
        // kronecker product matrices to become too small and the speed deteriorates drastically
        // in higher dimensions because reallocation is necessary. This can be prevented by
        // precomputing the number of nonzeros when preallocating memory (see myKroneckerProduct).
        int numKnotsLB = multiplicityTarget - basis.getKnotMultiplicity(dim, lb.at(dim));
        if (numKnotsLB > 0)
        {
            insertKnots(lb.at(dim), dim, numKnotsLB);
        }

        int numKnotsUB = multiplicityTarget - basis.getKnotMultiplicity(dim, ub.at(dim));
        if (numKnotsUB > 0)
        {
            insertKnots(ub.at(dim), dim, numKnotsUB);
        }
    }
}

bool BSpline::removeUnsupportedBasisFunctions(std::vector<double> &lb, std::vector<double> &ub)
{
    assert(lb.size() == numVariables);
    assert(ub.size() == numVariables);

    SparseMatrix A = basis.reduceSupport(lb, ub);

    if (coefficients.cols() != A.rows())
        return false;

    // Remove unsupported control points (basis functions)
    coefficients = coefficients*A;
    knotaverages = knotaverages*A;

    return true;
}

void BSpline::save(const std::string fileName) const
{
    Serializer s;
    s.serialize(*this);
    s.saveToFile(fileName);
}

void BSpline::load(const std::string fileName)
{
    Serializer s(fileName);
    s.deserialize(*this);
}

const std::string BSpline::getDescription() const
{
    std::string description("BSpline of degree");
    auto degrees = getBasisDegrees();
    // See if all degrees are the same.
    bool equal = true;
    for (size_t i = 1; i < degrees.size(); ++i)
    {
        equal = equal && (degrees.at(i) == degrees.at(i-1));
    }

    if(equal)
    {
        description.append(" ");
        description.append(std::to_string(degrees.at(0)));
    }
    else
    {
        description.append("s (");
        for (size_t i = 0; i < degrees.size(); ++i)
        {
            description.append(std::to_string(degrees.at(i)));
            if (i + 1 < degrees.size())
            {
                description.append(", ");
            }
        }
        description.append(")");
    }

    return description;
}

} // namespace SPLINTER
