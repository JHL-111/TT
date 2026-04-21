/**
 * @file Shape.cpp
 * @brief Implementation of the Shape class
 *
 * Implements all Shape class functionality, primarily by calling OpenCASCADE APIs.
 */

#include "cad_core/Shape.h"
#include <GProp_GProps.hxx>  // Geometry properties calculator
#include <BRepGProp.hxx>     // BRep geometry properties
#include <TopAbs.hxx>
#include <gp_Pnt.hxx>

namespace cad_core {

    /**
     * Default constructor - creates an empty shape
     */
    Shape::Shape() {

    }

    /**
     * Construct from an OpenCASCADE shape
     * The most common construction path - wraps a native shape
     * @param shape The OpenCASCADE shape to wrap
     */
    Shape::Shape(const TopoDS_Shape& shape) : m_shape(shape) {
    }

    /**
     * Get the underlying OpenCASCADE shape
     * @return Const reference to the internal shape
     */
    const TopoDS_Shape& Shape::GetOCCTShape() const {
        return m_shape;
    }

    /**
     * Set the underlying shape
     * Replaces the current shape with a new one
     * @param shape The new shape
     */
    void Shape::SetOCCTShape(const TopoDS_Shape& shape) {
        m_shape = shape;
    }

    /**
     * Check whether the shape is valid
     * An empty shape is equivalent to nothing 
     * @return true if valid, false if empty/null
     */
    bool Shape::IsValid() const {
        return !m_shape.IsNull();
    }

    /**
     * Compute the volume
     * Uses OpenCASCADE's geometry properties API
     * @return Volume value; returns 0 if the shape is invalid
     */
    double Shape::Volume() const {
        if (!IsValid()) {
            // An empty shape has zero volume
            return 0.0;
        }

        // OpenCASCADE geometry properties calculator
        GProp_GProps props;

        // Compute volumetric properties
        BRepGProp::VolumeProperties(m_shape, props);

        // Mass() actually returns volume here
        return props.Mass();
    }

    /**
     * Compute the surface area
     * @return Surface area value; returns 0 if the shape is invalid
     */
    double Shape::Area() const {
        if (!IsValid()) {
            // No shape means no surface - self-evident
            return 0.0;
        }

        GProp_GProps props;

        // Compute surface area properties
        BRepGProp::SurfaceProperties(m_shape, props);

        // Mass() returns the surface area in this context
        return props.Mass();
    }

    /**
     * Compute the geometric centroid / centre of mass.
     * Automatically selects the correct computation method based on shape type
     * (solid / face / wire).
     */
    gp_Pnt Shape::GetCentroid() const {
        if (!IsValid()) {
            return gp_Pnt(0.0, 0.0, 0.0);
        }

        GProp_GProps props;
        TopAbs_ShapeEnum type = m_shape.ShapeType();

        // Dispatch to the appropriate properties calculator based on topological dimension
        if (type == TopAbs_SOLID || type == TopAbs_COMPSOLID) {
            BRepGProp::VolumeProperties(m_shape, props);
        }
        else if (type == TopAbs_SHELL || type == TopAbs_FACE) {
            BRepGProp::SurfaceProperties(m_shape, props);
        }
        else if (type == TopAbs_WIRE || type == TopAbs_EDGE) {
            BRepGProp::LinearProperties(m_shape, props);
        }
        else {
            // For vertices or unsupported types, fall back to the origin
            return gp_Pnt(0.0, 0.0, 0.0);
        }

        return props.CentreOfMass();
    }

} // namespace cad_core