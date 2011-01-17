/****************************************************************************
* MeshLab                                                           o o     *
* A versatile mesh processing toolbox                             o     o   *
*                                                                _   O  _   *
* Copyright(C) 2005                                                \/)\/    *
* Visual Computing Lab                                            /\/|      *
* ISTI - Italian National Research Council                           |      *
*                                                                    \      *
* All rights reserved.                                                      *
*                                                                           *
* This program is free software; you can redistribute it and/or modify      *   
* it under the terms of the GNU General Public License as published by      *
* the Free Software Foundation; either version 2 of the License, or         *
* (at your option) any later version.                                       *
*                                                                           *
* This program is distributed in the hope that it will be useful,           *
* but WITHOUT ANY WARRANTY; without even the implied warranty of            *
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
* GNU General Public License (http://www.gnu.org/licenses/gpl.txt)          *
* for more details.                                                         *
*                                                                           *
****************************************************************************/

#ifndef _RASTERING_H
#define _RASTERING_H

#include <QtGui>
#include <common/interfaces.h>
#include <vcg/complex/trimesh/point_sampling.h>
#include <vcg/space/triangle2.h>

class VertexSampler
{
    typedef vcg::GridStaticPtr<CMeshO::FaceType, CMeshO::ScalarType > MetroMeshGrid;
    typedef vcg::tri::FaceTmark<CMeshO> MarkerFace;

    CMeshO &srcMesh;
    QImage &srcImg;
    float dist_upper_bound;

    MetroMeshGrid unifGridFace;
    MarkerFace markerFunctor;
    vcg::face::PointDistanceBaseFunctor<CMeshO::ScalarType> PDistFunct;

    // Callback stuff
    vcg::CallBackPos *cb;
    int vertexNo, vertexCnt, start, offset;

public:
    VertexSampler(CMeshO &_srcMesh, QImage &_srcImg, float upperBound) :
    srcMesh(_srcMesh), srcImg(_srcImg), dist_upper_bound(upperBound)
    {
        unifGridFace.Set(_srcMesh.face.begin(),_srcMesh.face.end());
        markerFunctor.SetMesh(&_srcMesh);
    }

    void InitCallback(vcg::CallBackPos *_cb, int _vertexNo, int _start=0, int _offset=100)
    {
        assert(_vertexNo > 0);
        assert(_start>=0);
        assert(_offset>=0 && _offset <= 100-_start);
        cb = _cb;
        vertexNo = _vertexNo;
        vertexCnt = 0;
        start = _start;
        offset = _offset;
    }

    void AddVert(CMeshO::VertexType &v)
    {
        // Get Closest point
        CMeshO::CoordType closestPt;
        float dist=dist_upper_bound;
        CMeshO::FaceType *nearestF;
        nearestF =  unifGridFace.GetClosest(PDistFunct, markerFunctor, v.cP(), dist_upper_bound, dist, closestPt);
        if (dist == dist_upper_bound) return;

        // Convert point to barycentric coords
        vcg::Point3f interp;
        int axis;
        if (nearestF->Flags() & CMeshO::FaceType::NORMX ) axis = 0;
        else if (nearestF->Flags() & CMeshO::FaceType::NORMY ) axis = 1;
        else axis = 2;
        bool ret = InterpolationParameters(*nearestF, axis, closestPt, interp);
        assert(ret);
        interp[2]=1.0-interp[1]-interp[0];

        int w=srcImg.width(), h=srcImg.height();
        int x, y;
        x = w * (interp[0]*nearestF->cWT(0).U()+interp[1]*nearestF->cWT(1).U()+interp[2]*nearestF->cWT(2).U());
        y = h * (1.0 - (interp[0]*nearestF->cWT(0).V()+interp[1]*nearestF->cWT(1).V()+interp[2]*nearestF->cWT(2).V()));
        // repeat mode
        x = (x%w + w)%w;
        y = (y%h + h)%h;
        QRgb px = srcImg.pixel(x, y);
        v.C() = CMeshO::VertexType::ColorType(qRed(px), qGreen(px), qBlue(px), 255);
    }
};

class RasterSampler
{
    QImage &trgImg;

    // Callback stuff
    vcg::CallBackPos *cb;
    const CMeshO::FaceType *currFace;
    int faceNo, faceCnt, start, offset;

public:
    RasterSampler(QImage &_img) : trgImg(_img) {}

    void InitCallback(vcg::CallBackPos *_cb, int _faceNo, int _start=0, int _offset=100)
    {
        assert(_faceNo > 0);
        assert(_start>=0);
        assert(_offset>=0 && _offset <= 100-_start);
        cb = _cb;
        faceNo = _faceNo;
        faceCnt = 0;
        start = _start;
        offset = _offset;
        currFace = NULL;
    }

        // expects points outside face (affecting face color) with edge distance > 0
    void AddTextureSample(const CMeshO::FaceType &f, const CMeshO::CoordType &p, const vcg::Point2i &tp, float edgeDist= 0.0)
    {
        CMeshO::VertexType::ColorType c;
        /*int alpha = 255;
        if (fabs(p[0]+p[1]+p[2]-1)>=0.00001)
            if (p[0] <.0) {alpha = 254+p[0]*128; bary[0] = 0.;} else
                if (p[1] <.0) {alpha = 254+p[1]*128; bary[1] = 0.;} else
                    if (p[2] <.0) {alpha = 254+p[2]*128; bary[2] = 0.;}*/
        int alpha = 255;
        if (edgeDist != 0.0)
            alpha=254-edgeDist*128;

        if (alpha==255 || qAlpha(trgImg.pixel(tp.X(), trgImg.height() - 1 - tp.Y())) < alpha)
        {
            c.lerp(f.V(0)->cC(), f.V(1)->cC(), f.V(2)->cC(), p);
            trgImg.setPixel(tp.X(), trgImg.height() - 1 - tp.Y(), qRgba(c[0], c[1], c[2], alpha));
        }
        if (cb)
        {
            if (&f != currFace) {currFace = &f; ++faceCnt;}
            cb(start + faceCnt*offset/faceNo, "Rasterizing faces ...");
        }
    }
};

class TransferColorSampler
{
    typedef vcg::GridStaticPtr<CMeshO::FaceType, CMeshO::ScalarType > MetroMeshGrid;
    typedef vcg::GridStaticPtr<CMeshO::VertexType, CMeshO::ScalarType > VertexMeshGrid;

    QImage &trgImg;
    QImage *srcImg;
    float dist_upper_bound;
    bool fromTexture;
    MetroMeshGrid unifGridFace;
    VertexMeshGrid   unifGridVert;
    bool useVertexSampling;

    // Callback stuff
    vcg::CallBackPos *cb;
    const CMeshO::FaceType *currFace;
    CMeshO *srcMesh;
    int faceNo, faceCnt, start, offset;

    typedef vcg::tri::FaceTmark<CMeshO> MarkerFace;
    MarkerFace markerFunctor;

    /*QRgb GetBilinearPixelColor(float _u, float _v, int alpha)
    {
        int w = srcImg->width();
        int h = srcImg->height();
        QRgb p00, p01, p10, p11;
        float u = _u * w -0.5;
        float v = _v * h -0.5;
        int x = floor(u);
        int y = floor(v);
        float u_ratio = u - x;
        float v_ratio = v - y;
        x = (x%w + w)%w;
        y = (y%h + h)%h;
        float u_opposite = 1 - u_ratio;
        float v_opposite = 1 - v_ratio;

        p00 = srcImg->pixel(x,y);
        p01 = srcImg->pixel(x, (y+1)%h);
        p10 = srcImg->pixel((x+1)%w, y);
        p11 = srcImg->pixel((x+1)%w, (y+1)%h);
        int r,g,b;
        r = (qRed(p00)*u_opposite+qRed(p01)*u_ratio)*v_opposite +
            (qRed(p01)*u_opposite+qRed(p11)*u_ratio)*v_ratio;
        g = (qGreen(p00)*u_opposite+qGreen(p01)*u_ratio)*v_opposite +
            (qGreen(p01)*u_opposite+qGreen(p11)*u_ratio)*v_ratio;
        b = (qBlue(p00)*u_opposite+qBlue(p01)*u_ratio)*v_opposite +
            (qBlue(p01)*u_opposite+qBlue(p11)*u_ratio)*v_ratio;
        return qRgba(r,g,b, alpha);
    }*/

public:
    TransferColorSampler(CMeshO &_srcMesh, QImage &_trgImg, float upperBound)
    : trgImg(_trgImg), dist_upper_bound(upperBound)
    {
        srcMesh=&_srcMesh;
        useVertexSampling = _srcMesh.face.empty();
        if(useVertexSampling) unifGridVert.Set(_srcMesh.vert.begin(),_srcMesh.vert.end());
                        else  unifGridFace.Set(_srcMesh.face.begin(),_srcMesh.face.end());
        markerFunctor.SetMesh(&_srcMesh);
        fromTexture = false;

    }

    TransferColorSampler(CMeshO &_srcMesh, QImage &_trgImg, QImage *_srcImg, float upperBound)
    : trgImg(_trgImg), dist_upper_bound(upperBound)
    {
        assert(_srcImg != NULL);
        srcImg = _srcImg;
        unifGridFace.Set(_srcMesh.face.begin(),_srcMesh.face.end());
        markerFunctor.SetMesh(&_srcMesh);
        fromTexture = true;
        useVertexSampling=false;
    }

    void InitCallback(vcg::CallBackPos *_cb, int _faceNo, int _start=0, int _offset=100)
    {
        assert(_faceNo > 0);
        assert(_start>=0);
        assert(_offset>=0 && _offset <= 100-_start);
        cb = _cb;
        faceNo = _faceNo;
        faceCnt = 0;
        start = _start;
        offset = _offset;
        currFace = NULL;
    }

    void AddTextureSample(const CMeshO::FaceType &f, const CMeshO::CoordType &p, const vcg::Point2i &tp, float edgeDist=0.0)
    {
        // Calculate correct barycentric coords
        /*CMeshO::CoordType bary = p;
        int alpha = 255;
        if (fabs(p[0]+p[1]+p[2]-1)>=0.00001)
            if (p[0] <.0) {alpha = 254+p[0]*128; bary[0] = 0.;} else
                if (p[1] <.0) {alpha = 254+p[1]*128; bary[1] = 0.;} else
                    if (p[2] <.0) {alpha = 254+p[2]*128; bary[2] = 0.;}*/

        CMeshO::CoordType bary = p;
        int alpha = 255;
        if (edgeDist != 0.0)
            alpha=254-edgeDist*128;

        // Get point on face
        CMeshO::CoordType startPt;
        startPt[0] = bary[0]*f.V(0)->P().X()+bary[1]*f.V(1)->P().X()+bary[2]*f.V(2)->P().X();
        startPt[1] = bary[0]*f.V(0)->P().Y()+bary[1]*f.V(1)->P().Y()+bary[2]*f.V(2)->P().Y();
        startPt[2] = bary[0]*f.V(0)->P().Z()+bary[1]*f.V(1)->P().Z()+bary[2]*f.V(2)->P().Z();

        // Retrieve closest point on source mesh

        if(useVertexSampling)
        {
            CMeshO::VertexType   *nearestV=0;
            float dist=dist_upper_bound;
            nearestV =  vcg::tri::GetClosestVertex<CMeshO,VertexMeshGrid>(*srcMesh,unifGridVert,startPt,dist_upper_bound,dist); //(PDistFunct,markerFunctor,startPt,dist_upper_bound,dist,closestPt);
        //if(cb) cb(sampleCnt++*100/sampleNum,"Resampling Vertex attributes");
            //if(storeDistanceAsQualityFlag)  p.Q() = dist;
            if(dist == dist_upper_bound) return ;
            trgImg.setPixel(tp.X(), trgImg.height() - 1 - tp.Y(), qRgba(nearestV->C()[0], nearestV->C()[1], nearestV->C()[2], 255));
        }
        else // sampling from a mesh
        {
            CMeshO::CoordType closestPt;
            vcg::face::PointDistanceBaseFunctor<CMeshO::ScalarType> PDistFunct;
            float dist=dist_upper_bound;
            CMeshO::FaceType *nearestF;
            nearestF =  unifGridFace.GetClosest(PDistFunct, markerFunctor, startPt, dist_upper_bound, dist, closestPt);
            if (dist == dist_upper_bound) return;

            // Convert point to barycentric coords
            vcg::Point3f interp;
            int axis;
            if (nearestF->Flags() & CMeshO::FaceType::NORMX ) axis = 0;
            else if (nearestF->Flags() & CMeshO::FaceType::NORMY ) axis = 1;
            else axis = 2;
            bool ret = InterpolationParameters(*nearestF, axis, closestPt, interp);
						// if the point is outside the nearest face,
						// then let's simply use the color of the nearest vertex:
						if(!ret)
						{
							CMeshO::VertexType *nearestV=0;
							float dist=dist_upper_bound;
							nearestV =  vcg::tri::GetClosestVertex<CMeshO,VertexMeshGrid>(*srcMesh,unifGridVert,startPt,dist_upper_bound,dist);
							if(dist == dist_upper_bound) return ;
							trgImg.setPixel(tp.X(), trgImg.height() - 1 - tp.Y(), qRgba(nearestV->C()[0], nearestV->C()[1], nearestV->C()[2], 255));
							return;
						}
            interp[2]=1.0-interp[1]-interp[0];

        if (alpha==255 || qAlpha(trgImg.pixel(tp.X(), trgImg.height() - 1 - tp.Y())) < alpha)
            if (fromTexture)
            {
                int w=srcImg->width(), h=srcImg->height();
                int x, y;
                x = w * (interp[0]*nearestF->cWT(0).U()+interp[1]*nearestF->cWT(1).U()+interp[2]*nearestF->cWT(2).U());
                y = h * (1.0 - (interp[0]*nearestF->cWT(0).V()+interp[1]*nearestF->cWT(1).V()+interp[2]*nearestF->cWT(2).V()));
                // texture repeat mode
                x = (x%w + w)%w;
                y = (y%h + h)%h;
                QRgb px = srcImg->pixel(x, y);
                trgImg.setPixel(tp.X(), trgImg.height() - 1 - tp.Y(), qRgba(qRed(px), qGreen(px), qBlue(px), alpha));
            }
            else
            {
                // Calculate and set color
                CMeshO::VertexType::ColorType c;
                c.lerp(nearestF->V(0)->cC(), nearestF->V(1)->cC(), nearestF->V(2)->cC(), interp);
                trgImg.setPixel(tp.X(), trgImg.height() - 1 - tp.Y(), qRgba(c[0], c[1], c[2], alpha));
            }

            if (cb)
            {
                if (&f != currFace) {currFace = &f; ++faceCnt;}
                cb(start + faceCnt*offset/faceNo, "Rasterizing faces ...");
            }
        }
    }
};

#endif
