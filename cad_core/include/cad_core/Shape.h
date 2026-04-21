/**
 * This class wraps OpenCASCADE's TopoDS_Shape.
 */

#pragma once

#include <TopoDS_Shape.hxx>
#include <memory>
#include <gp_Pnt.hxx>

namespace cad_core {

    /**
     * @class Shape
     * @brief Wrapper class for geometric shapes
     *
     */
    class Shape {
    public:
        // Default constructor
        Shape();

        /**
         * Construct from an OpenCASCADE shape
         * @param shape Native OpenCASCADE shape
         */
        explicit Shape(const TopoDS_Shape& shape);

        /** Virtual destructor */
        virtual ~Shape() = default;

        /**
         * Get the underlying OpenCASCADE shape
         * @return Reference to the TopoDS_Shape
         */
        const TopoDS_Shape& GetOCCTShape() const;

        /**
         * Set the underlying shape
         * @param shape New OpenCASCADE shape
         */
        void SetOCCTShape(const TopoDS_Shape& shape);

        /**
         * Check whether the shape is valid
         * @return true if the shape is valid, false if it is empty/null
         */
        bool IsValid() const;

        /**
         * Compute the volume
         * @return Volume value; units depend on the modelling units in use
         * TODO: Add unit handling and error checking
         */
        double Volume() const;

        /**
         * Compute the surface area
         * @return Surface area value
         * TODO: May require special handling for non-closed shapes
         */
        double Area() const;

        /**
         * Compute the geometric centroid/centre of mass.
         * Automatically selects the correct method based on shape type (solid / face / wire).
         * @return Centroid coordinates as a point
         */
        gp_Pnt GetCentroid() const;

    private:
        /** Stores the actual OpenCASCADE shape */
        TopoDS_Shape m_shape;
    };

    /** Smart pointer type alias */
    using ShapePtr = std::shared_ptr<Shape>;

} // namespace cad_core