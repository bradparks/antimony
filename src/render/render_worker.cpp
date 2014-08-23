#include <boost/python.hpp>
#include <boost/format.hpp>

#include <QApplication>
#include <QDebug>
#include <QTime>

#include "render/render_worker.h"
#include "render/render_image.h"

#include "cpp/shape.h"
#include "cpp/transform.h"

using namespace boost::python;

RenderWorker::RenderWorker(PyObject *s, QMatrix4x4 matrix,
                           float scale, int refinement)
    : QObject(NULL), shape(s), matrix(matrix),
      scale(scale), refinement(refinement), image(NULL)
{
    Py_INCREF(shape);
}

RenderWorker::~RenderWorker()
{
    Py_DECREF(shape);
}

RenderWorker* RenderWorker::getNext() const
{
    return refinement
        ? new RenderWorker(shape, matrix, scale*2, refinement - 1)
        : NULL;
}

void RenderWorker::render()
{
    extract<Shape> get_shape(shape);

    Q_ASSERT(get_shape.check());
    Shape s = get_shape();

    Q_ASSERT(!(isinf(s.bounds.zmin) ^ isinf(s.bounds.zmax)));

    if (!isinf(s.bounds.xmin) && !isinf(s.bounds.xmax) &&
        !isinf(s.bounds.xmin) && !isinf(s.bounds.xmax))
    {
        if (isinf(s.bounds.zmin))
        {
            render2d(s);
        }
        else
        {
            render3d(s);
        }
    }

    emit(finished());
}

void RenderWorker::render3d(Shape s)
{
    Transform T = getTransform(matrix);
    Shape transformed = s.map(T);

    image = new RenderImage(
            transformed.bounds,
            matrix.inverted() * QVector3D(transformed.bounds.xmin,
                                          transformed.bounds.ymax,
                                          transformed.bounds.zmax),
            scale);
    image->render(&transformed);
    image->moveToThread(QApplication::instance()->thread());
}

void RenderWorker::render2d(Shape s)
{
    QMatrix4x4 matrix_flat = matrix;
    matrix_flat(0, 2) = 0;
    matrix_flat(1, 2) = 0;
    matrix_flat(2, 0) = 0;
    matrix_flat(2, 1) = 0;
    matrix_flat(2, 2) = 1;

    Shape s_flat(s.math, s.bounds.xmin, s.bounds.ymin, 0,
                         s.bounds.xmax, s.bounds.ymax, 0);

    Transform T_flat = getTransform(matrix_flat);
    Shape transformed = s_flat.map(T_flat);

    // Render the flattened shape, but with bounds equivalent to the shape's
    // position in a 3D bounding box.
    Bounds b3d = Bounds(s.bounds.xmin, s.bounds.ymin, 0,
                        s.bounds.xmax, s.bounds.ymax, 0.0001).
                 map(getTransform(matrix));
    image = new RenderImage(
            b3d,
            matrix.inverted() *
                QVector3D(b3d.xmin, b3d.ymax, b3d.zmax),
            scale);
    image->render(&transformed);
    image->moveToThread(QApplication::instance()->thread());

    if (matrix(1,2))
    {
        image->applyGradient(matrix(2,2) > 0);
    }
}

Transform RenderWorker::getTransform(QMatrix4x4 m)
{
    QMatrix4x4 mf = m.inverted();
    QMatrix4x4 mi = mf.inverted();

    Transform T = Transform(
                (boost::format("++*Xf%g*Yf%g*Zf%g") %
                    mf(0,0) % mf(0,1) % mf(0,2)).str(),
                (boost::format("++*Xf%g*Yf%g*Zf%g") %
                    mf(1,0) % mf(1,1) % mf(1,2)).str(),
                (boost::format("++*Xf%g*Yf%g*Zf%g") %
                    mf(2,0) % mf(2,1) % mf(2,2)).str(),
                (boost::format("++*Xf%g*Yf%g*Zf%g") %
                    mi(0,0) % mi(0,1) % mi(0,2)).str(),
                (boost::format("++*Xf%g*Yf%g*Zf%g") %
                    mi(1,0) % mi(1,1) % mi(1,2)).str(),
                (boost::format("++*Xf%g*Yf%g*Zf%g") %
                    mi(2,0) % mi(2,1) % mi(2,2)).str());

    return T;
}
