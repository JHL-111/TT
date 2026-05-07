#include "cad_sketch/Sketch.h"
#include "cad_sketch/SketchLine.h"
#include "cad_sketch/SketchCircle.h"
#include "cad_sketch/SketchArc.h"
#include "cad_sketch/SketchCurve.h"

#include <iostream>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <TopoDS_Wire.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS.hxx>
#include <gp_Pnt.hxx>
#include <gp_Dir.hxx>
#include <gp_Ax2.hxx>
#include <gp_Ax3.hxx>
#include <gp_Circ.hxx>
#include <gp_Pln.hxx>
#include <algorithm>
#include <fstream>
#include <TopTools_HSequenceOfShape.hxx>
#include <ShapeAnalysis_FreeBounds.hxx>
#include <GeomAPI_Interpolate.hxx>
#include <TColgp_HArray1OfPnt.hxx>
#include <Geom_BSplineCurve.hxx>
#include <BOPAlgo_Builder.hxx>
#include <BOPAlgo_BuilderFace.hxx>
#include <TopTools_ListOfShape.hxx>
#include <TopExp_Explorer.hxx>
#include <BRepTools.hxx>
#include <Precision.hxx>
#include <GProp_GProps.hxx>
#include <BRepGProp.hxx>
#include <BRepCheck_Wire.hxx>
#include <BRepCheck_Status.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRep_Tool.hxx>
#include <TopExp.hxx>
#include <ShapeAnalysis_Wire.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <GCPnts_AbscissaPoint.hxx>
#include <GeomAPI_ProjectPointOnCurve.hxx>
#include <Geom_Curve.hxx>
#include <ShapeAnalysis_Curve.hxx>
#include <GeomAPI_ExtremaCurveCurve.hxx>
#include <BRep_Builder.hxx>
#include <map>
#include <set>
#include <numeric>

// Internal helper: converts a local 2D sketch point to a world 3D coordinate
// and builds an OCC Edge from a sketch element.
static TopoDS_Shape CreateEdgeFromElement(const cad_sketch::SketchElementPtr& elem, const gp_Ax3& cs) {
    auto Sketch2DToWorld = [](const cad_sketch::SketchPointPtr& pt, const gp_Ax3& ax) {
        return ax.Location().Translated(gp_Vec(ax.XDirection()) * pt->GetX() + gp_Vec(ax.YDirection()) * pt->GetY());
        };

    if (auto line = std::dynamic_pointer_cast<cad_sketch::SketchLine>(elem)) {
        gp_Pnt p1 = Sketch2DToWorld(line->GetStartPoint(), cs);
        gp_Pnt p2 = Sketch2DToWorld(line->GetEndPoint(), cs);
        if (p1.Distance(p2) > Precision::Confusion()) {
            return BRepBuilderAPI_MakeEdge(p1, p2);
        }
    }
    else if (auto circle = std::dynamic_pointer_cast<cad_sketch::SketchCircle>(elem)) {
        gp_Pnt center = Sketch2DToWorld(circle->GetCenter(), cs);
        gp_Ax2 ax2(center, cs.Direction(), cs.XDirection());
        return BRepBuilderAPI_MakeEdge(gp_Circ(ax2, circle->GetRadius()));
    }
    else if (auto arc = std::dynamic_pointer_cast<cad_sketch::SketchArc>(elem)) {
        gp_Pnt center = Sketch2DToWorld(arc->GetCenter(), cs);
        gp_Ax2 ax2(center, cs.Direction(), cs.XDirection());
        return BRepBuilderAPI_MakeEdge(gp_Circ(ax2, arc->GetRadius()), arc->GetStartAngle(), arc->GetEndAngle());
    }
    else if (auto curve = std::dynamic_pointer_cast<cad_sketch::SketchCurve>(elem)) {
        const auto& rawPoints = curve->GetControlPoints();
        std::vector<gp_Pnt> validPoints;
        for (const auto& pt : rawPoints) {
            gp_Pnt worldPt = Sketch2DToWorld(pt, cs);
            if (validPoints.empty() || validPoints.back().Distance(worldPt) > 1e-5) {
                validPoints.push_back(worldPt);
            }
        }

        if (validPoints.size() >= 2) {
            Handle(TColgp_HArray1OfPnt) occPoints = new TColgp_HArray1OfPnt(1, validPoints.size());
            for (size_t i = 0; i < validPoints.size(); ++i) occPoints->SetValue(i + 1, validPoints[i]);

            try {
                GeomAPI_Interpolate interpolator(occPoints, Standard_False, Precision::Confusion());
                interpolator.Perform();
                if (interpolator.IsDone()) {
                    return BRepBuilderAPI_MakeEdge(interpolator.Curve());
                }
            }
            catch (...) {
                return TopoDS_Shape();
            }
        }
    }
    return TopoDS_Shape();
}

// ============================================================================
// Validate whether a face is a truly valid closed profile.
// Three-stage validation:
//   1. The outer boundary wire must be closed.
//   2. Strict BRepCheck validation.
//   3. The area must be finite and reasonable.
// ============================================================================
static bool IsValidClosedProfile(const TopoDS_Face& face, double tolerance) {
    // 1. The outer boundary wire must exist
    TopoDS_Wire outerWire = BRepTools::OuterWire(face);
    if (outerWire.IsNull()) return false;

    // 2. Topological closure check
    if (!outerWire.Closed()) return false;

    // 3. Strict BRepCheck closure validation (checks that start and end vertices truly coincide)
    BRepCheck_Wire checker(outerWire);
    if (checker.Closed() != BRepCheck_NoError) return false;

    // 4. Iterate over all vertices in the outer wire and verify that the wire is truly connected.
    //    (Guards against "pseudo-closed" faces produced by BOPAlgo_BuilderFace
    //     that have finite UV bounds but are not spatially closed.)
    TopTools_IndexedDataMapOfShapeListOfShape vertexEdgeMap;
    TopExp::MapShapesAndAncestors(outerWire, TopAbs_VERTEX, TopAbs_EDGE, vertexEdgeMap);
    for (Standard_Integer i = 1; i <= vertexEdgeMap.Extent(); ++i) {
        // Each vertex must connect exactly 2 edges; otherwise the wire is open at that vertex
        if (vertexEdgeMap(i).Extent() < 2) {
            return false;
        }
    }

    // 5. The area must be finite and reasonable
    //    (filters out the infinite "background face" that spans the universe)
    GProp_GProps props;
    BRepGProp::SurfaceProperties(face, props);
    double area = props.Mass();
    if (area < Precision::Confusion() || area > 1e10) return false;

    return true;
}

// ============================================================================
// Pre-processing: split edges at all intersection points (T-type and X-type),
// then merge coincident vertices.
//   T-type: one edge's endpoint lies in the middle of another edge.
//   X-type: two edges intersect somewhere in the middle of each.
// ============================================================================
static TopTools_ListOfShape SplitEdgesAtAllIntersections(const TopTools_ListOfShape& edgeList, double tolerance) {
    std::vector<TopoDS_Edge> edges;
    for (TopTools_ListIteratorOfListOfShape it(edgeList); it.More(); it.Next()) {
        edges.push_back(TopoDS::Edge(it.Value()));
    }

    // key = edge index, value = list of parameter values at which to split
    std::map<int, std::vector<double>> splitParams;

    int n = (int)edges.size();
    for (int i = 0; i < n; ++i) {
        Standard_Real fi, li;
        Handle(Geom_Curve) ci = BRep_Tool::Curve(edges[i], fi, li);
        if (ci.IsNull()) continue;

        TopoDS_Vertex vi1, vi2;
        TopExp::Vertices(edges[i], vi1, vi2);
        gp_Pnt pi1 = vi1.IsNull() ? gp_Pnt() : BRep_Tool::Pnt(vi1);
        gp_Pnt pi2 = vi2.IsNull() ? gp_Pnt() : BRep_Tool::Pnt(vi2);
        double marginI = (li - fi) * 0.01;

        for (int j = i + 1; j < n; ++j) {
            Standard_Real fj, lj;
            Handle(Geom_Curve) cj = BRep_Tool::Curve(edges[j], fj, lj);
            if (cj.IsNull()) continue;

            TopoDS_Vertex vj1, vj2;
            TopExp::Vertices(edges[j], vj1, vj2);
            gp_Pnt pj1 = vj1.IsNull() ? gp_Pnt() : BRep_Tool::Pnt(vj1);
            gp_Pnt pj2 = vj2.IsNull() ? gp_Pnt() : BRep_Tool::Pnt(vj2);
            double marginJ = (lj - fj) * 0.01;

            // ---- X-type intersection: two edges cross in their interiors ----
            try {
                GeomAPI_ExtremaCurveCurve extrema(ci, cj, fi, li, fj, lj);
                for (int k = 1; k <= extrema.NbExtrema(); ++k) {
                    gp_Pnt p1, p2;
                    extrema.Points(k, p1, p2);
                    if (p1.Distance(p2) > tolerance) continue;

                    double paramI, paramJ;
                    extrema.Parameters(k, paramI, paramJ);

                    gp_Pnt ptOnI = ci->Value(paramI);
                    gp_Pnt ptOnJ = cj->Value(paramJ);

                    // Check whether the intersection lies in the interior of edge i (not at an endpoint)
                    bool atEndpointI = (!vi1.IsNull() && ptOnI.Distance(pi1) < tolerance) ||
                        (!vi2.IsNull() && ptOnI.Distance(pi2) < tolerance);
                    if (!atEndpointI && paramI > fi + marginI && paramI < li - marginI) {
                        splitParams[i].push_back(paramI);
                    }

                    // Check whether the intersection lies in the interior of edge j (not at an endpoint)
                    bool atEndpointJ = (!vj1.IsNull() && ptOnJ.Distance(pj1) < tolerance) ||
                        (!vj2.IsNull() && ptOnJ.Distance(pj2) < tolerance);
                    if (!atEndpointJ && paramJ > fj + marginJ && paramJ < lj - marginJ) {
                        splitParams[j].push_back(paramJ);
                    }
                }
            }
            catch (...) {}

            // ---- T-type intersection: an endpoint of edge j lies in the interior of edge i ----
            auto checkTJunction = [&](const gp_Pnt& pt, int targetIdx,
                const Handle(Geom_Curve)& curve,
                Standard_Real first, Standard_Real last,
                const gp_Pnt& ep1, const gp_Pnt& ep2,
                const TopoDS_Vertex& tv1, const TopoDS_Vertex& tv2,
                double margin) {
                    if (!tv1.IsNull() && pt.Distance(BRep_Tool::Pnt(tv1)) < tolerance) return;
                    if (!tv2.IsNull() && pt.Distance(BRep_Tool::Pnt(tv2)) < tolerance) return;

                    GeomAPI_ProjectPointOnCurve proj(pt, curve, first, last);
                    if (proj.NbPoints() > 0 && proj.LowerDistance() < tolerance) {
                        double param = proj.LowerDistanceParameter();
                        if (param > first + margin && param < last - margin) {
                            splitParams[targetIdx].push_back(param);
                        }
                    }
                };

            // Endpoints of edge j projected onto edge i
            if (!vj1.IsNull()) checkTJunction(pj1, i, ci, fi, li, pi1, pi2, vi1, vi2, marginI);
            if (!vj2.IsNull() && !vj1.IsSame(vj2)) checkTJunction(pj2, i, ci, fi, li, pi1, pi2, vi1, vi2, marginI);

            // Endpoints of edge i projected onto edge j
            if (!vi1.IsNull()) checkTJunction(pi1, j, cj, fj, lj, pj1, pj2, vj1, vj2, marginJ);
            if (!vi2.IsNull() && !vi1.IsSame(vi2)) checkTJunction(pi2, j, cj, fj, lj, pj1, pj2, vj1, vj2, marginJ);
        }
    }

    // ---- Perform the splits ----
    TopTools_ListOfShape result;
    for (int i = 0; i < n; ++i) {
        auto it = splitParams.find(i);
        if (it == splitParams.end() || it->second.empty()) {
            result.Append(edges[i]);
            continue;
        }

        std::vector<double>& params = it->second;
        std::sort(params.begin(), params.end());
        params.erase(std::unique(params.begin(), params.end(),
            [](double a, double b) { return std::abs(a - b) < 1e-9; }), params.end());

        Standard_Real eFirst, eLast;
        Handle(Geom_Curve) curve = BRep_Tool::Curve(edges[i], eFirst, eLast);

        double prevParam = eFirst;
        for (double p : params) {
            if (p - prevParam > 1e-9) {
                try {
                    BRepBuilderAPI_MakeEdge me(curve, prevParam, p);
                    if (me.IsDone()) result.Append(me.Edge());
                }
                catch (...) {}
            }
            prevParam = p;
        }
        if (eLast - prevParam > 1e-9) {
            try {
                BRepBuilderAPI_MakeEdge me(curve, prevParam, eLast);
                if (me.IsDone()) result.Append(me.Edge());
            }
            catch (...) {}
        }
    }

    return result;
}

// ============================================================================
// Merge coincident vertices: make all edges at the same geometric location
// share the same TopoDS_Vertex, so that BOPAlgo_BuilderFace can correctly
// assemble closed wires.
// ============================================================================
static TopTools_ListOfShape ShareVerticesInEdges(const TopTools_ListOfShape& edgeList, double tolerance) {
    struct SharedVtx {
        gp_Pnt point;
        TopoDS_Vertex vertex;
    };
    std::vector<SharedVtx> vtxPool;

    auto findOrMakeVertex = [&](const gp_Pnt& pt) -> TopoDS_Vertex {
        for (auto& sv : vtxPool) {
            if (sv.point.Distance(pt) < tolerance) return sv.vertex;
        }
        BRep_Builder bb;
        TopoDS_Vertex v;
        bb.MakeVertex(v, pt, Precision::Confusion());
        vtxPool.push_back({ pt, v });
        return v;
        };

    TopTools_ListOfShape result;
    for (TopTools_ListIteratorOfListOfShape it(edgeList); it.More(); it.Next()) {
        TopoDS_Edge edge = TopoDS::Edge(it.Value());
        TopoDS_Vertex v1, v2;
        TopExp::Vertices(edge, v1, v2);

        Standard_Real first, last;
        Handle(Geom_Curve) curve = BRep_Tool::Curve(edge, first, last);

        if (curve.IsNull() || v1.IsNull()) {
            result.Append(edge);
            continue;
        }

        gp_Pnt p1 = BRep_Tool::Pnt(v1);
        TopoDS_Vertex sv1 = findOrMakeVertex(p1);

        if (v2.IsNull() || v1.IsSame(v2)) {
            // Closed edge (circle)
            try {
                BRepBuilderAPI_MakeEdge me(curve, sv1, sv1, first, last);
                if (me.IsDone()) { result.Append(me.Edge()); continue; }
            }
            catch (...) {}
            result.Append(edge);
            continue;
        }

        gp_Pnt p2 = BRep_Tool::Pnt(v2);
        TopoDS_Vertex sv2 = findOrMakeVertex(p2);

        try {
            BRepBuilderAPI_MakeEdge me(curve, sv1, sv2, first, last);
            if (me.IsDone()) { result.Append(me.Edge()); continue; }
        }
        catch (...) {}
        result.Append(edge);
    }
    return result;
}


namespace cad_sketch {

    Sketch::Sketch() : m_name("Sketch") {
    }

    Sketch::Sketch(const std::string& name) : m_name(name) {
    }

    const std::string& Sketch::GetName() const {
        return m_name;
    }

    void Sketch::SetName(const std::string& name) {
        m_name = name;
    }

    void Sketch::AddElement(const SketchElementPtr& element) {
        m_elements.push_back(element);
    }

    void Sketch::ClearElements() {
        m_elements.clear();
    }

    void Sketch::RemoveElement(const SketchElementPtr& element) {
        auto it = std::find(m_elements.begin(), m_elements.end(), element);
        if (it != m_elements.end()) {
            m_elements.erase(it);
        }
    }

    const std::vector<SketchElementPtr>& Sketch::GetElements() const {
        return m_elements;
    }

    SketchElementPtr Sketch::GetElementById(int id) const {
        for (const auto& element : m_elements) {
            if (element->GetId() == id) {
                return element;
            }
        }
        return nullptr;
    }

    void Sketch::AddConstraint(const ConstraintPtr& constraint) {
        m_constraints.push_back(constraint);
        m_solver.AddConstraint(constraint);
    }

    void Sketch::RemoveConstraint(const ConstraintPtr& constraint) {
        auto it = std::find(m_constraints.begin(), m_constraints.end(), constraint);
        if (it != m_constraints.end()) {
            m_constraints.erase(it);
            m_solver.RemoveConstraint(constraint);
        }
    }

    void Sketch::ClearConstraints() {
        m_constraints.clear();
        m_solver.ClearConstraints();
    }

    const std::vector<ConstraintPtr>& Sketch::GetConstraints() const {
        return m_constraints;
    }

    bool Sketch::SolveConstraints() {
        return m_solver.Solve();
    }

    bool Sketch::ValidateConstraints() const {
        return m_solver.ValidateConstraints();
    }

    void Sketch::SelectElement(const SketchElementPtr& element) {
        element->SetSelected(true);
    }

    void Sketch::DeselectElement(const SketchElementPtr& element) {
        element->SetSelected(false);
    }

    void Sketch::ClearSelection() {
        for (auto& element : m_elements) {
            element->SetSelected(false);
        }
    }

    std::vector<SketchElementPtr> Sketch::GetSelectedElements() const {
        std::vector<SketchElementPtr> selected;
        for (const auto& element : m_elements) {
            if (element->IsSelected()) {
                selected.push_back(element);
            }
        }
        return selected;
    }

    bool Sketch::IsEmpty() const {
        return m_elements.empty();
    }

    int Sketch::GetElementCount() const {
        return static_cast<int>(m_elements.size());
    }

    int Sketch::GetConstraintCount() const {
        return static_cast<int>(m_constraints.size());
    }



    void Sketch::UpdateProfiles(const gp_Ax3& cs) {
        m_profiles.clear();

        const double tolerance = 0.1;

        // ── 1. Generate all edges ──
        TopTools_ListOfShape edgeList;
        for (const auto& elem : m_elements) {
            TopoDS_Shape shape = CreateEdgeFromElement(elem, cs);
            if (!shape.IsNull() && shape.ShapeType() == TopAbs_EDGE) {
                edgeList.Append(shape);
            }
        }
        if (edgeList.IsEmpty()) return;

        // ── 2. Split edges at all intersections (T-type and X-type) ──
        TopTools_ListOfShape splitEdges = SplitEdgesAtAllIntersections(edgeList, tolerance);

        // ── 3. Merge coincident vertices to establish topological connectivity ──
        TopTools_ListOfShape connectedEdges = ShareVerticesInEdges(splitEdges, tolerance);

        // Collect edges into a vector
        std::vector<TopoDS_Edge> edgeVec;
        for (TopTools_ListIteratorOfListOfShape it(connectedEdges); it.More(); it.Next()) {
            edgeVec.push_back(TopoDS::Edge(it.Value()));
        }
        if (edgeVec.empty()) return;

        // ── 4. Build a half-edge data structure (planar subdivision) ──
        // 4a. Collect unique vertices and their 2D coordinates
        struct VtxInfo {
            gp_Pnt pt3d;
            double x2d, y2d;
            std::vector<int> outHalfEdges; // indices of half-edges leaving this vertex
        };
        std::vector<VtxInfo> vertices;

        auto findVertex = [&](const gp_Pnt& pt) -> int {
            for (int i = 0; i < (int)vertices.size(); ++i) {
                if (vertices[i].pt3d.Distance(pt) < tolerance) return i;
            }
            gp_Vec v(cs.Location(), pt);
            double x = v.Dot(gp_Vec(cs.XDirection()));
            double y = v.Dot(gp_Vec(cs.YDirection()));
            vertices.push_back({ pt, x, y, {} });
            return (int)vertices.size() - 1;
            };

        // 4b. Build half-edges
        struct HalfEdge {
            int edgeIdx;    // index in edgeVec
            bool forward;   // true = along the curve's positive direction
            int startVtx;
            int endVtx;
            int twin;       // half-edge in the opposite direction of the same edge
            int next;       // next half-edge in the same face loop
            bool visited;
        };
        std::vector<HalfEdge> halfEdges;

        // Process open edges (edges with two distinct endpoints)
        std::vector<int> circleEdgeIndices; // closed edges (circles) handled separately
        for (int ei = 0; ei < (int)edgeVec.size(); ++ei) {
            TopoDS_Edge& edge = edgeVec[ei];
            TopoDS_Vertex v1, v2;
            TopExp::Vertices(edge, v1, v2);
            if (v1.IsNull() || v2.IsNull()) continue;

            if (v1.IsSame(v2)) {
                // Closed edge (circle) — handled separately below
                circleEdgeIndices.push_back(ei);
                continue;
            }

            Standard_Real first, last;
            Handle(Geom_Curve) curve = BRep_Tool::Curve(edge, first, last);
            if (curve.IsNull()) continue;

            gp_Pnt p1 = curve->Value(first);
            gp_Pnt p2 = curve->Value(last);
            int vi1 = findVertex(p1);
            int vi2 = findVertex(p2);

            int fwdIdx = (int)halfEdges.size();
            halfEdges.push_back({ ei, true, vi1, vi2, fwdIdx + 1, -1, false });
            int revIdx = (int)halfEdges.size();
            halfEdges.push_back({ ei, false, vi2, vi1, fwdIdx, -1, false });

            vertices[vi1].outHalfEdges.push_back(fwdIdx);
            vertices[vi2].outHalfEdges.push_back(revIdx);
        }

        // 4c. Compute the departure tangent angle for each half-edge leaving its start vertex
        auto halfEdgeAngle = [&](int heIdx) -> double {
            HalfEdge& he = halfEdges[heIdx];
            TopoDS_Edge& edge = edgeVec[he.edgeIdx];
            Standard_Real first, last;
            Handle(Geom_Curve) curve = BRep_Tool::Curve(edge, first, last);

            gp_Pnt pt;
            gp_Vec tangent;
            if (he.forward) {
                curve->D1(first, pt, tangent);
            }
            else {
                curve->D1(last, pt, tangent);
                tangent.Reverse();
            }

            double dx = tangent.Dot(gp_Vec(cs.XDirection()));
            double dy = tangent.Dot(gp_Vec(cs.YDirection()));
            return atan2(dy, dx);
            };

        // 4d. Sort outgoing half-edges at each vertex by angle (counter-clockwise)
        for (auto& vtx : vertices) {
            std::sort(vtx.outHalfEdges.begin(), vtx.outHalfEdges.end(),
                [&](int a, int b) { return halfEdgeAngle(a) < halfEdgeAngle(b); });
        }

        // 4e. Link the next pointers (left-turn rule for face loop traversal).
        // Half-edge h goes from A→B. On arriving at B, the next half-edge in the
        // same face loop is the one that makes the smallest left turn from h's
        // arrival direction. Algorithmically: find h's twin (B→A) in B's CCW-sorted
        // outgoing list; the entry immediately preceding the twin in CCW order is
        // the next half-edge. This rule traces each interior face counter-clockwise,
        // which produces a positive signed area when the resulting polygon is
        // evaluated by the shoelace formula in step 5. The unbounded outer face,
        // traced in clockwise direction, yields a negative signed area and is
        // discarded.
        for (int hi = 0; hi < (int)halfEdges.size(); ++hi) {
            HalfEdge& he = halfEdges[hi];
            int arriveVtx = he.endVtx;
            int twinIdx = he.twin;

            auto& outList = vertices[arriveVtx].outHalfEdges;
            // Find the position of twin in outList
            int twinPos = -1;
            for (int k = 0; k < (int)outList.size(); ++k) {
                if (outList[k] == twinIdx) { twinPos = k; break; }
            }
            if (twinPos < 0) { he.next = -1; continue; }

            // The entry one position before the twin in CCW order is the next
            // half-edge in the clockwise (interior) face loop
            int prevPos = (twinPos - 1 + (int)outList.size()) % (int)outList.size();
            he.next = outList[prevPos];
        }

        // ── 5. Traverse all face loops and collect candidate faces ──
        struct FaceCandidate {
            std::vector<int> loopHEs;           // half-edge indices
            std::vector<std::pair<double, double>> poly2D; // 2D vertex coordinates
            double signedArea;
            double cx, cy;                      // centroid
        };
        std::vector<FaceCandidate> candidates;

        for (int hi = 0; hi < (int)halfEdges.size(); ++hi) {
            if (halfEdges[hi].visited) continue;
            if (halfEdges[hi].next < 0) continue;

            std::vector<int> loop;
            int cur = hi;
            bool valid = true;
            while (true) {
                if (halfEdges[cur].visited) {
                    if (cur == hi) break;
                    valid = false;
                    break;
                }
                halfEdges[cur].visited = true;
                loop.push_back(cur);
                cur = halfEdges[cur].next;
                if (cur < 0) { valid = false; break; }
                if (cur == hi) break;
                if ((int)loop.size() > (int)halfEdges.size()) { valid = false; break; }
            }

            if (!valid || loop.size() < 3) continue;

            // Collect 2D vertices; compute signed area and centroid
            FaceCandidate fc;
            fc.loopHEs = loop;
            fc.signedArea = 0.0;
            fc.cx = 0.0;
            fc.cy = 0.0;
            for (int heIdx : loop) {
                HalfEdge& he = halfEdges[heIdx];
                VtxInfo& vs = vertices[he.startVtx];
                fc.poly2D.push_back({ vs.x2d, vs.y2d });
                fc.cx += vs.x2d;
                fc.cy += vs.y2d;
                VtxInfo& ve = vertices[he.endVtx];
                fc.signedArea += (vs.x2d * ve.y2d - ve.x2d * vs.y2d);
            }
            fc.signedArea *= 0.5;
            fc.cx /= (double)loop.size();
            fc.cy /= (double)loop.size();

            double absArea = std::abs(fc.signedArea);
            if (absArea < Precision::Confusion() || absArea > 1e10) continue;

            // Keep only counter-clockwise loops (positive area = interior face).
            // Clockwise loops are the boundary of the outer infinite face — discard them.
            if (fc.signedArea <= 0) continue;

            candidates.push_back(std::move(fc));
        }

        // ── 5b. Add closed edges (circles) as polygon candidates ──
        // To allow closed circles to participate in nested-hierarchy detection,
        // each closed edge is sampled into a 32-point polygon and added to the
        // candidate list, alongside the polygons formed by half-edge loops.
        struct ClosedEdgeRef {
            int edgeIdx;        // index in edgeVec
            int candidateIdx;   // index into candidates after insertion
        };
        std::vector<ClosedEdgeRef> closedEdgeRefs;

        for (int ci : circleEdgeIndices) {
            TopoDS_Edge& edge = edgeVec[ci];
            Standard_Real first, last;
            Handle(Geom_Curve) curve = BRep_Tool::Curve(edge, first, last);
            if (curve.IsNull()) continue;

            const int NSAMPLES = 32;
            FaceCandidate fc;
            fc.signedArea = 0.0;
            fc.cx = 0.0;
            fc.cy = 0.0;

            std::vector<std::pair<double, double>> samples;
            samples.reserve(NSAMPLES);
            for (int k = 0; k < NSAMPLES; ++k) {
                double t = first + (last - first) * (double)k / (double)NSAMPLES;
                gp_Pnt pt = curve->Value(t);
                gp_Vec v(cs.Location(), pt);
                double x = v.Dot(gp_Vec(cs.XDirection()));
                double y = v.Dot(gp_Vec(cs.YDirection()));
                samples.push_back({ x, y });
            }
            // Compute signed area of sampled polygon
            for (int k = 0; k < NSAMPLES; ++k) {
                int kn = (k + 1) % NSAMPLES;
                fc.signedArea += (samples[k].first * samples[kn].second
                    - samples[kn].first * samples[k].second);
                fc.cx += samples[k].first;
                fc.cy += samples[k].second;
            }
            fc.signedArea *= 0.5;
            fc.cx /= (double)NSAMPLES;
            fc.cy /= (double)NSAMPLES;

            // Skip degenerate or absurdly large samples
            double absArea = std::abs(fc.signedArea);
            if (absArea < Precision::Confusion() || absArea > 1e10) continue;

            fc.poly2D = samples;
            // Note: for closed circles, signed area sign depends on parametric
            // direction; we keep both orientations and let the hierarchy logic
            // treat them uniformly via the absolute polygon shape.
            // Mark this candidate as a closed edge by leaving loopHEs empty.

            int candIdx = (int)candidates.size();
            closedEdgeRefs.push_back({ ci, candIdx });
            candidates.push_back(std::move(fc));
        }

        // ── 5c. Build containment relation using all-vertices test ──
        // candidate i contains candidate j  ⇔
        //   every vertex of j's polygon lies strictly inside i's polygon
        // (more robust than centroid testing on concave shapes)
        auto pointInPolygon = [](double px, double py,
            const std::vector<std::pair<double, double>>& poly) -> bool {
                int n = (int)poly.size();
                bool inside = false;
                for (int i = 0, j = n - 1; i < n; j = i++) {
                    double xi = poly[i].first, yi = poly[i].second;
                    double xj = poly[j].first, yj = poly[j].second;
                    if (((yi > py) != (yj > py)) &&
                        (px < (xj - xi) * (py - yi) / (yj - yi) + xi)) {
                        inside = !inside;
                    }
                }
                return inside;
            };

        auto polygonContains = [&](int i, int j) -> bool {
            if (i == j) return false;
            const auto& outer = candidates[i].poly2D;
            const auto& inner = candidates[j].poly2D;
            if (outer.size() < 3 || inner.size() < 3) return false;
            // Quick reject by area: a smaller polygon cannot contain a larger one
            if (std::abs(candidates[i].signedArea) <=
                std::abs(candidates[j].signedArea)) return false;
            for (const auto& vtx : inner) {
                if (!pointInPolygon(vtx.first, vtx.second, outer)) return false;
            }
            return true;
            };

        int nCand = (int)candidates.size();
        // parents[i] = list of candidates that contain i
        std::vector<std::vector<int>> parents(nCand);
        for (int i = 0; i < nCand; ++i) {
            for (int j = 0; j < nCand; ++j) {
                if (polygonContains(j, i)) {
                    parents[i].push_back(j);
                }
            }
        }

        // ── 5d. Resolve direct (immediate) parent for each candidate ──
        // i's direct parent = the candidate in parents[i] with the smallest area
        // (i.e. the most tightly enclosing one). Equivalently, the one not
        // contained by any other parent.
        std::vector<int> directParent(nCand, -1);
        for (int i = 0; i < nCand; ++i) {
            int best = -1;
            double bestArea = std::numeric_limits<double>::max();
            for (int p : parents[i]) {
                double a = std::abs(candidates[p].signedArea);
                if (a < bestArea) { bestArea = a; best = p; }
            }
            directParent[i] = best;
        }

        // ── 5e. Compute nesting depth via parent chain ──
        std::vector<int> depth(nCand, 0);
        for (int i = 0; i < nCand; ++i) {
            int d = 0;
            int cur = directParent[i];
            int safety = 0;
            while (cur != -1 && safety++ < nCand) {
                d++;
                cur = directParent[cur];
            }
            depth[i] = d;
        }

        // ── 5f. Build faces grouped by even-odd rule ──
        // Even depth (0, 2, 4...) = outer wire of a new face
        // Odd  depth (1, 3, 5...) = hole inside its direct parent face
        // Each odd-depth candidate is attached as a hole to its direct parent.

        // Helper: build a TopoDS_Wire from a candidate.
        // For half-edge candidates: walk loopHEs.
        // For closed-edge candidates (loopHEs empty): use the original closed edge.
        auto buildWire = [&](int candIdx, bool reverse) -> TopoDS_Wire {
            BRepBuilderAPI_MakeWire wireMaker;
            const FaceCandidate& fc = candidates[candIdx];

            if (!fc.loopHEs.empty()) {
                // Half-edge derived loop
                if (!reverse) {
                    for (int heIdx : fc.loopHEs) {
                        const HalfEdge& he = halfEdges[heIdx];
                        TopoDS_Edge edge = edgeVec[he.edgeIdx];
                        if (!he.forward) edge.Reverse();
                        wireMaker.Add(edge);
                    }
                }
                else {
                    // Build in reverse order with each edge flipped
                    for (auto it = fc.loopHEs.rbegin(); it != fc.loopHEs.rend(); ++it) {
                        const HalfEdge& he = halfEdges[*it];
                        TopoDS_Edge edge = edgeVec[he.edgeIdx];
                        if (he.forward) edge.Reverse();
                        wireMaker.Add(edge);
                    }
                }
            }
            else {
                // Closed edge (circle) candidate
                int realEdgeIdx = -1;
                for (const auto& cer : closedEdgeRefs) {
                    if (cer.candidateIdx == candIdx) { realEdgeIdx = cer.edgeIdx; break; }
                }
                if (realEdgeIdx < 0) return TopoDS_Wire();
                TopoDS_Edge edge = edgeVec[realEdgeIdx];
                if (reverse) edge.Reverse();
                wireMaker.Add(edge);
            }

            if (!wireMaker.IsDone()) return TopoDS_Wire();
            return wireMaker.Wire();
            };

        gp_Pln sketchPlane(cs);

        // Group children by their direct parent for hole attachment
        std::vector<std::vector<int>> childrenOf(nCand);
        for (int i = 0; i < nCand; ++i) {
            if (directParent[i] >= 0 && (depth[i] % 2 == 1)) {
                childrenOf[directParent[i]].push_back(i);
            }
        }

        // For every even-depth candidate, build a face with its odd-depth
        // direct children as holes.
        for (int i = 0; i < nCand; ++i) {
            if (depth[i] % 2 != 0) continue; // only even depths become outer faces

            TopoDS_Wire outerWire = buildWire(i, false);
            if (outerWire.IsNull()) continue;

            BRepBuilderAPI_MakeFace faceMaker(sketchPlane, outerWire, Standard_True);
            if (!faceMaker.IsDone()) continue;

            // Attach each direct child (odd depth) as a hole.
            // OCC requires inner wires to be oriented opposite to the outer wire,
            // so we build them reversed.
            for (int childIdx : childrenOf[i]) {
                TopoDS_Wire holeWire = buildWire(childIdx, true);
                if (holeWire.IsNull()) continue;
                faceMaker.Add(holeWire);
            }

            if (!faceMaker.IsDone()) continue;
            TopoDS_Face face = faceMaker.Face();

            GProp_GProps props;
            BRepGProp::SurfaceProperties(face, props);
            double area = props.Mass();
            if (area < Precision::Confusion() || area > 1e10) continue;

            m_profiles.push_back(std::make_shared<SketchProfile>(face));
        }

        // ── 6. Fallback: handle independent closed contours not covered above ──
        // (e.g. open polylines that close via shared endpoints but were not
        // captured as half-edge loops, due to degenerate topology.)
        if (m_profiles.empty()) {
            Handle(TopTools_HSequenceOfShape) allEdges = new TopTools_HSequenceOfShape();
            for (TopTools_ListIteratorOfListOfShape it(connectedEdges); it.More(); it.Next()) {
                allEdges->Append(it.Value());
            }

            Handle(TopTools_HSequenceOfShape) wires = new TopTools_HSequenceOfShape();
            ShapeAnalysis_FreeBounds::ConnectEdgesToWires(allEdges, tolerance,
                Standard_False, wires);

            for (Standard_Integer i = 1; i <= wires->Length(); ++i) {
                TopoDS_Wire wire = TopoDS::Wire(wires->Value(i));
                if (!wire.Closed()) continue;

                BRepBuilderAPI_MakeFace makeFace(sketchPlane, wire, Standard_True);
                if (makeFace.IsDone()) {
                    TopoDS_Face face = makeFace.Face();

                    GProp_GProps props;
                    BRepGProp::SurfaceProperties(face, props);
                    double area = props.Mass();
                    if (area < Precision::Confusion() || area > 1e10) continue;

                    m_profiles.push_back(std::make_shared<SketchProfile>(face));
                }
            }
        }
    }
      

} // namespace cad_sketch