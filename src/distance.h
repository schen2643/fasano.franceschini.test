// Functions to compute the distance used to compute the Fasano-Franceschini test statistic

#ifndef DISTANCE_H
#define DISTANCE_H

#include <vector>
#include <limits>
#include "RangeTree.h"
#include "matrix_util.h"

typedef RangeTree::RangeTree<double, double> RTree;
typedef RangeTree::Point<double, double> Point;

// Build range trees for two samples
//
// @param M matrix of two data samples row binded
// @param r1 number of points in first sample
// @param r2 number of points in second sample
// @param s a permutation of {0,1,...,(r1+r2-1)} which indicates which points belong to
// each sample
// @return range trees built on each sample
template<typename MatrixT>
std::vector<RTree> buildRangeTrees(const MatrixT& M,
                                   std::vector<double> w,
                                   std::size_t r1,
                                   std::size_t r2,
                                   const std::vector<std::size_t>& s) {
    std::vector<Point> pts1;
    std::vector<Point> pts2;
    pts1.reserve(r1);
    pts2.reserve(r2);

    // Extract points from first sample
    for (std::size_t i = 0; i < r1; ++i) {
        pts1.push_back(Point(getRow<MatrixT>(M, s[i]), w[i]));
    }

    // Extract points from second sample
    for (std::size_t i = 0; i < r2; ++i) {
        pts2.push_back(Point(getRow<MatrixT>(M, s[i + r1]), w[i + r1]));
    }

    // Construct range trees
    return {RTree(pts1), RTree(pts2)};
}

// Evaluate the distance function using the range tree method.
//
// @param rtree1 range tree for the first sample
// @param rtree2 range tree for the second sample
// @param n1 the number of points in the first sample
// @param n2 the number of points in the second sample
// @param origin vector
// @return long
long rangeDistance(const RTree& rtree1,
                   const RTree& rtree2,
                   std::size_t n1,
                   std::size_t n2,
                   const std::vector<double>& origin) {
    std::size_t ndim = origin.size();
    std::vector<bool> strict(ndim, false);
    double inf = std::numeric_limits<double>::infinity();

    long d = 0;
    for (std::size_t i = 0; i < (1 << ndim); ++i) {
        // Determine the upper and lower bounds on each dimension for the given orthant.
        // That is, the orthant can be represented as the Cartesian product of the
        // intervals (lowerLims[j], upperLims[j]) for j = 0, 1,..., ndim-1.
        std::vector<double> lowerLims(ndim), upperLims(ndim);
        for (std::size_t j = 0; j < ndim; ++j) {
            // The orthants in R^d can be enumerated as all length d combinations of <
            // and >. Representing < as 0 and > as 1, these combinations are given by
            // the rows of the (2^d) x d matrix whose i-th row is the integer i in
            // binary. The (i,j) element of this matrix is given by (i & (1 << (d-1-j))).
            if (i & (1 << (ndim - 1 - j))) {
                lowerLims[j] = -inf;
                upperLims[j] = origin[j];
            } else {
                lowerLims[j] = origin[j];
                upperLims[j] = inf;
            }
        }
        long res1 = static_cast<long>(n2 * rtree1.countInRange(lowerLims, upperLims,
                                                               strict, strict));
        long res2 = static_cast<long>(n1 * rtree2.countInRange(lowerLims, upperLims,
                                                               strict, strict));
        d = std::max(d, std::abs(res1 - res2));
    }
    return d;
}


// Find the octant relative to the specified origin that a point lies in.
//
// @param pt the point
// @param origin the origin
// @return the octant the point lies in. If there is a tie along one coordinate,
// returns 0.
std::size_t findOct(const std::vector<double>& pt,
                    const std::vector<double>& origin) {
    std::size_t ndim = pt.size();
    std::size_t oct = 0;
    for (std::size_t i = 0; i < ndim; ++i) {
        if (pt[i] > origin[i]) {
            oct += 1 << (ndim - i - 1);
        } else if (pt[i] == origin[i]) {
            // Ties are not counted
            return 0;
        }
    }
    return oct + 1;
}


// Evaluate the distance function using the brute force method.
//
// @param M a matrix of type NumericMatrix or RMatrix<double>
// @param n1 the number of points in the first sample
// @param n2 the number of points in the second sample
// @param s a permutation of {0, 1, ..., (r1+r2-1)} which indicates which points
// belong to each sample
// @param origin_ix row of M to use as the origin
// @return long
template<typename MatrixT>
long bruteDistance(const MatrixT& M,
                   std::size_t n1,
                   std::size_t n2,
                   const std::vector<std::size_t>& s,
                   std::size_t origin_ix) {
    std::vector<double> origin = getRow<MatrixT>(M, s[origin_ix]);
    std::size_t ndim = origin.size();
    long d = 0;

    long l1 = static_cast<long>(n1);
    long l2 = static_cast<long>(n2);

    // If the dimension of the data is low, use a std::vector to tabulate points per
    // octant. But if the dimension of the data is high, most octants will be empty so
    // we instead use a std::unordered_map.
    const std::size_t MAXDIM = 13;
    if (ndim <= MAXDIM) {
        std::size_t noct = 1 << ndim;
        std::vector<long> counts1(noct + 1), counts2(noct + 1);
        for (std::size_t i = 0; i < n1; ++i) {
            ++counts1[findOct(getRow<MatrixT>(M, s[i]), origin)];
        }
        for (std::size_t i = 0; i < n2; ++i) {
            ++counts2[findOct(getRow<MatrixT>(M, s[i + n1]), origin)];
        }
        // Note that oct = 0 is used as a sink for uncounted points
        for (std::size_t i = 1; i <= noct; ++i) {
            d = std::max(d, std::abs(l2 * counts1[i] - l1 * counts2[i]));
        }
    } else {
        std::unordered_map<std::size_t, long> counts1, counts2;
        for (std::size_t i = 0; i < n1; ++i) {
            counts1[findOct(getRow<MatrixT>(M, s[i]), origin)] += l2;
        }
        for (std::size_t i = 0; i < n2; ++i) {
            counts2[findOct(getRow<MatrixT>(M, s[i + n1]), origin)] += l1;
        }

        for (const auto& kv : counts2) {
            counts1[kv.first] = std::abs(counts1[kv.first] - kv.second);
        }
        for (const auto& kv : counts1) {
            // Note that oct = 0 is used as a sink for uncounted points
            if (kv.first != 0) {
                d = std::max(d, kv.second);
            }
        }
    }
    return d;
}

#endif //DISTANCE_H
