#ifndef COLORMAP_H
#define COLORMAP_H

#include <cmath>
#include <vector>
#include <algorithm>
#include <numeric>


#include <QStringList>
#include <QColor>
#include "qthelper.h"

class ColorMap {
    static std::vector<ColorMap*> sm_colormaps;

public:
    ColorMap(){
        sm_colormaps.push_back(this);
    }
    virtual QString name()=0;
    virtual QRgb map(float y)=0;
    inline QRgb operator ()(float y) {return map(y);}
    static QStringList getAvailableColorMaps() {
        QStringList list;
        for(std::vector<ColorMap*>::iterator it=sm_colormaps.begin(); it!=sm_colormaps.end(); ++it)
            list.append((*it)->name());
        return list;
    }
    static ColorMap& getAt(int index) {
        return (*sm_colormaps.at(index));
    }
};

class ColorMapGray : public  ColorMap {
public:
    virtual QString name() {return "Gray";}
    virtual QRgb map(float y){
        y = 1.0-y;

        int color = 255;
        if(!qIsInf(y))
            color = 255*y;

        if(color<0) color = 0;
        else if(color>255) color = 255;

        return qRgb(color, color, color);
    }
};

class ColorMapJet : public  ColorMap {
public:
    virtual QString name() {return "Jet";}
    virtual QRgb map(float y){
        y = 1.0-y;
        float red = 0;
        if(y<0.25) red = 4*y+0.5;
        else       red = -4*y+2.5;
        if(red>1.0)      red=1.0;
        else if(red<0.0) red=0.0;

        float green = 0;
        if(y<0.5)  green = 4*y-0.5;
        else       green = -4*y+3.5;
        if(green>1.0)      green=1.0;
        else if(green<0.0) green=0.0;

        float blue = 0;
        if(y<0.75) blue = 4*y-1.5;
        else       blue = -4*y+4.5;
        if(blue>1.0)      blue=1.0;
        else if(blue<0.0) blue=0.0;

        return qRgb(int(255*red), int(255*green), int(255*blue));
    }
};

#endif // COLORMAP_H
