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

// 这是一个将局部 2D 点转为世界 3D 坐标并生成 OCC Edge 的内部辅助函数
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
// 验证一个 face 是否是真正有效的封闭轮廓
// 三层验证：1.外环线框闭合  2.BRepCheck严格验证  3.面积有限且合理
// ============================================================================
static bool IsValidClosedProfile(const TopoDS_Face& face, double tolerance) {
    // 1. 外环线框必须存在
    TopoDS_Wire outerWire = BRepTools::OuterWire(face);
    if (outerWire.IsNull()) return false;

    // 2. 拓扑闭合检查
    if (!outerWire.Closed()) return false;

    // 3. BRepCheck 严格闭合性验证（检查首尾顶点是否真正重合）
    BRepCheck_Wire checker(outerWire);
    if (checker.Closed() != BRepCheck_NoError) return false;

    // 4. 遍历外环的所有顶点，检查线框是否真正首尾相连
    //    （防止 BOPAlgo_BuilderFace 产出的"假闭合"面 —— UV 有限但空间不封闭）
    TopTools_IndexedDataMapOfShapeListOfShape vertexEdgeMap;
    TopExp::MapShapesAndAncestors(outerWire, TopAbs_VERTEX, TopAbs_EDGE, vertexEdgeMap);
    for (Standard_Integer i = 1; i <= vertexEdgeMap.Extent(); ++i) {
        // 每个顶点必须恰好连接 2 条边，否则线框在该点是开放的
        if (vertexEdgeMap(i).Extent() < 2) {
            return false;
        }
    }

    // 5. 面积必须有限且合理（过滤掉无穷大的"宇宙背景面"）
    GProp_GProps props;
    BRepGProp::SurfaceProperties(face, props);
    double area = props.Mass();
    if (area < Precision::Confusion() || area > 1e10) return false;

    return true;
}

// ============================================================================
// 预处理：在所有交叉处（T型 + X型）切割边，然后合并重合顶点
// T型：一条边的端点落在另一条边的中间
// X型：两条边在各自的中间部位相交
// ============================================================================
static TopTools_ListOfShape SplitEdgesAtAllIntersections(const TopTools_ListOfShape& edgeList, double tolerance) {
    std::vector<TopoDS_Edge> edges;
    for (TopTools_ListIteratorOfListOfShape it(edgeList); it.More(); it.Next()) {
        edges.push_back(TopoDS::Edge(it.Value()));
    }

    // key = 边的索引, value = 需要切割的参数列表
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

            // ---- X型交叉：两条边中间部位相交 ----
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

                    // 判断是否在边 i 的内部（不在端点处）
                    bool atEndpointI = (!vi1.IsNull() && ptOnI.Distance(pi1) < tolerance) ||
                        (!vi2.IsNull() && ptOnI.Distance(pi2) < tolerance);
                    if (!atEndpointI && paramI > fi + marginI && paramI < li - marginI) {
                        splitParams[i].push_back(paramI);
                    }

                    // 判断是否在边 j 的内部（不在端点处）
                    bool atEndpointJ = (!vj1.IsNull() && ptOnJ.Distance(pj1) < tolerance) ||
                        (!vj2.IsNull() && ptOnJ.Distance(pj2) < tolerance);
                    if (!atEndpointJ && paramJ > fj + marginJ && paramJ < lj - marginJ) {
                        splitParams[j].push_back(paramJ);
                    }
                }
            }
            catch (...) {}

            // ---- T型交叉：边 j 的端点落在边 i 的内部 ----
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

            // 边 j 的端点 → 投影到边 i
            if (!vj1.IsNull()) checkTJunction(pj1, i, ci, fi, li, pi1, pi2, vi1, vi2, marginI);
            if (!vj2.IsNull() && !vj1.IsSame(vj2)) checkTJunction(pj2, i, ci, fi, li, pi1, pi2, vi1, vi2, marginI);

            // 边 i 的端点 → 投影到边 j
            if (!vi1.IsNull()) checkTJunction(pi1, j, cj, fj, lj, pj1, pj2, vj1, vj2, marginJ);
            if (!vi2.IsNull() && !vi1.IsSame(vi2)) checkTJunction(pi2, j, cj, fj, lj, pj1, pj2, vj1, vj2, marginJ);
        }
    }

    // ---- 执行切割 ----
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
// 合并重合顶点：让同一几何位置的所有边共享同一个 TopoDS_Vertex
// 这样 BOPAlgo_BuilderFace 才能正确连成封闭 wire
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
            // 封闭边（圆）
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

        // ── 1. 生成所有边 ──
        TopTools_ListOfShape edgeList;
        for (const auto& elem : m_elements) {
            TopoDS_Shape shape = CreateEdgeFromElement(elem, cs);
            if (!shape.IsNull() && shape.ShapeType() == TopAbs_EDGE) {
                edgeList.Append(shape);
            }
        }
        if (edgeList.IsEmpty()) return;

        // ── 2. 在所有交叉处切割边（T型 + X型）──
        TopTools_ListOfShape splitEdges = SplitEdgesAtAllIntersections(edgeList, tolerance);

        // ── 3. 合并重合顶点，建立拓扑连接 ──
        TopTools_ListOfShape connectedEdges = ShareVerticesInEdges(splitEdges, tolerance);

        // 收集边到 vector
        std::vector<TopoDS_Edge> edgeVec;
        for (TopTools_ListIteratorOfListOfShape it(connectedEdges); it.More(); it.Next()) {
            edgeVec.push_back(TopoDS::Edge(it.Value()));
        }
        if (edgeVec.empty()) return;

        // ── 4. 构建半边数据结构（planar subdivision）──
        // 4a. 收集唯一顶点及其 2D 坐标
        struct VtxInfo {
            gp_Pnt pt3d;
            double x2d, y2d;
            std::vector<int> outHalfEdges; // 从该顶点出发的半边索引
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

        // 4b. 构建半边
        struct HalfEdge {
            int edgeIdx;    // 在 edgeVec 中的索引
            bool forward;   // 沿曲线正方向
            int startVtx;
            int endVtx;
            int twin;       // 同一条边反方向的半边
            int next;       // 同一个面循环中的下一条半边
            bool visited;
        };
        std::vector<HalfEdge> halfEdges;

        // 处理开放边（有两个不同端点的边）
        std::vector<int> circleEdgeIndices; // 单独处理封闭边（圆）
        for (int ei = 0; ei < (int)edgeVec.size(); ++ei) {
            TopoDS_Edge& edge = edgeVec[ei];
            TopoDS_Vertex v1, v2;
            TopExp::Vertices(edge, v1, v2);
            if (v1.IsNull() || v2.IsNull()) continue;

            if (v1.IsSame(v2)) {
                // 封闭边（圆），后面单独处理
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

        // 4c. 计算每条半边离开起始顶点的切线角度
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

        // 4d. 每个顶点的出边按角度逆时针排序
        for (auto& vtx : vertices) {
            std::sort(vtx.outHalfEdges.begin(), vtx.outHalfEdges.end(),
                [&](int a, int b) { return halfEdgeAngle(a) < halfEdgeAngle(b); });
        }

        // 4e. 链接 next 指针
        // 半边 h 从 A→B，到达 B 后，找 h 的 twin（从 B→A）在 B 的出边列表中的位置，
        // 逆时针排序中 twin 的前一个出边就是该面循环的下一条半边。
        for (int hi = 0; hi < (int)halfEdges.size(); ++hi) {
            HalfEdge& he = halfEdges[hi];
            int arriveVtx = he.endVtx;
            int twinIdx = he.twin;

            auto& outList = vertices[arriveVtx].outHalfEdges;
            // 找 twin 在 outList 中的位置
            int twinPos = -1;
            for (int k = 0; k < (int)outList.size(); ++k) {
                if (outList[k] == twinIdx) { twinPos = k; break; }
            }
            if (twinPos < 0) { he.next = -1; continue; }

            // 前一个（逆时针排序中 twin 的前一个 = 顺时针方向的下一个）
            int prevPos = (twinPos - 1 + (int)outList.size()) % (int)outList.size();
            he.next = outList[prevPos];
        }

        // ── 5. 遍历所有面循环，收集候选面 ──
        struct FaceCandidate {
            std::vector<int> loopHEs;          // 半边索引
            std::vector<std::pair<double, double>> poly2D; // 顶点2D坐标
            double signedArea;
            double cx, cy;                     // 质心
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

            // 收集 2D 顶点、计算有符号面积和质心
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

            // 只保留逆时针循环（正面积 = 内部面）
            // 负面积循环是外部无穷大面的边界，直接丢弃
            if (fc.signedArea <= 0) continue;

            candidates.push_back(std::move(fc));
        }

        // ── 5b. 去除外边界面 ──
        // 如果一个面包含了其他面的质心，说明它是外边界（覆盖了其他面），应该被移除。
        // 使用射线法做2D点在多边形内的判定。
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

        std::vector<bool> isRedundant(candidates.size(), false);
        for (int i = 0; i < (int)candidates.size(); ++i) {
            if (isRedundant[i]) continue;
            int containedCount = 0;
            for (int j = 0; j < (int)candidates.size(); ++j) {
                if (i == j || isRedundant[j]) continue;
                if (pointInPolygon(candidates[j].cx, candidates[j].cy, candidates[i].poly2D)) {
                    containedCount++;
                }
            }
            // 如果面 i 包含了至少一个其他面的质心，说明 i 是外边界
            if (containedCount > 0) {
                isRedundant[i] = true;
            }
        }

        // ── 5c. 从非冗余候选构建实际面 ──
        gp_Pln sketchPlane(cs);

        for (int i = 0; i < (int)candidates.size(); ++i) {
            if (isRedundant[i]) continue;

            BRepBuilderAPI_MakeWire wireMaker;
            for (int heIdx : candidates[i].loopHEs) {
                HalfEdge& he = halfEdges[heIdx];
                TopoDS_Edge edge = edgeVec[he.edgeIdx];
                if (!he.forward) edge.Reverse();
                wireMaker.Add(edge);
            }
            if (!wireMaker.IsDone()) continue;

            TopoDS_Wire wire = wireMaker.Wire();

            BRepBuilderAPI_MakeFace faceMaker(sketchPlane, wire, Standard_True);
            if (faceMaker.IsDone()) {
                TopoDS_Face face = faceMaker.Face();

                GProp_GProps props;
                BRepGProp::SurfaceProperties(face, props);
                double area = props.Mass();
                if (area < Precision::Confusion() || area > 1e10) continue;

                m_profiles.push_back(std::make_shared<SketchProfile>(face));
            }
        }

        // ── 6. 处理封闭边（圆等）──
        for (int ci : circleEdgeIndices) {
            TopoDS_Edge& edge = edgeVec[ci];
            BRepBuilderAPI_MakeWire wireMaker;
            wireMaker.Add(edge);
            if (!wireMaker.IsDone()) continue;

            TopoDS_Wire wire = wireMaker.Wire();
            BRepBuilderAPI_MakeFace faceMaker(sketchPlane, wire, Standard_True);
            if (faceMaker.IsDone()) {
                TopoDS_Face face = faceMaker.Face();

                GProp_GProps props;
                BRepGProp::SurfaceProperties(face, props);
                double area = props.Mass();
                if (area < Precision::Confusion() || area > 1e10) continue;

                m_profiles.push_back(std::make_shared<SketchProfile>(face));
            }
        }

        // ── 7. 处理未被半边遍历覆盖的独立封闭轮廓 ──
        // （如独立矩形等 —— 上面的半边算法已涵盖，这里做兜底）
        if (m_profiles.empty()) {
            Handle(TopTools_HSequenceOfShape) allEdges = new TopTools_HSequenceOfShape();
            for (TopTools_ListIteratorOfListOfShape it(connectedEdges); it.More(); it.Next()) {
                allEdges->Append(it.Value());
            }

            Handle(TopTools_HSequenceOfShape) wires = new TopTools_HSequenceOfShape();
            ShapeAnalysis_FreeBounds::ConnectEdgesToWires(allEdges, tolerance, Standard_False, wires);

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