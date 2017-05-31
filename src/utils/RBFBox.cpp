/*---------------------------------------------------------------------------*\
 *
 *  mimmo
 *
 *  Copyright (C) 2015-2017 OPTIMAD engineering Srl
 *
 *  -------------------------------------------------------------------------
 *  License
 *  This file is part of mimmo.
 *
 *  mimmo is free software: you can redistribute it and/or modify it
 *  under the terms of the GNU Lesser General Public License v3 (LGPL)
 *  as published by the Free Software Foundation.
 *
 *  mimmo is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
 *  License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with mimmo. If not, see <http://www.gnu.org/licenses/>.
 *
 \ *---------------------------------------------------------------------------*/

#include "RBFBox.hpp"
#include "LinearAlgebra.hpp"
#include "lapacke.h"

#include <chrono>

using namespace std::chrono;

using namespace std;
namespace mimmo{


/*! Basic Constructor.*/
RBFBox::RBFBox(){
    m_name = "mimmo.RBFBox";
    m_origin.fill(0.0);
    m_span.fill(1.0);
    int counter = 0;
    for(auto &val : m_axes)    {
        val.fill(0.0);
        val[counter] = 1.0;
        ++counter;
    }
    m_nodes.clear();
    m_suppR = 0.0;
};

/*!
 * Custom constructor reading xml data
 * \param[in] rootXML reference to your xml tree section
 */
RBFBox::RBFBox(const bitpit::Config::Section & rootXML){

    m_name = "mimmo.RBFBox";
    m_origin.fill(0.0);
    m_span.fill(1.0);
    int counter = 0;
    for(auto &val : m_axes)    {
        val.fill(0.0);
        val[counter] = 1.0;
        ++counter;
    }
    m_nodes.clear();
    m_suppR = 0.0;


    std::string fallback_name = "ClassNONE";
    std::string input = rootXML.get("ClassName", fallback_name);
    input = bitpit::utils::trim(input);
    if(input == "mimmo.RBFBox"){
        absorbSectionXML(rootXML);
    }else{
        std::cout<<"Warning in custom xml mimmo::RBFBox constructor. No valid xml data found"<<std::endl;
    };
}

/*! Destructor */
RBFBox::~RBFBox(){};

/*! Copy Constructor
 *\param[in] other RBFBox where copy from
 */
RBFBox::RBFBox(const RBFBox & other):BaseManipulation(){
    *this = other;
};

/*! Copy Operator
 * \param[in] other RBFBox where copy from
 */
RBFBox & RBFBox::operator=(const RBFBox & other){

    *(static_cast<BaseManipulation *>(this))  = *(static_cast<const BaseManipulation *>(&other));
    m_origin = other.m_origin;
    m_span   = other.m_span;
    m_axes = other.m_axes;
    m_nodes = other.m_nodes;
    m_suppR = other.m_suppR;
    return(*this);
};

/*! It builds the input/output ports of the object
 */
void
RBFBox::buildPorts(){

    bool built = true;

    built = (built && createPortIn<dvecarr3E, RBFBox>(this, &mimmo::RBFBox::setNode, PortType::M_COORDS, mimmo::pin::containerTAG::VECARR3, mimmo::pin::dataTAG::FLOAT));
    built = (built && createPortIn<double, RBFBox>(this, &mimmo::RBFBox::setSupportRadius, PortType::M_VALUED, mimmo::pin::containerTAG::SCALAR, mimmo::pin::dataTAG::FLOAT));

    built = (built && createPortOut<darray3E, RBFBox>(this, &mimmo::RBFBox::getOrigin, PortType::M_POINT, mimmo::pin::containerTAG::ARRAY3, mimmo::pin::dataTAG::FLOAT));
    built = (built && createPortOut<dmatrix33E, RBFBox>(this, &mimmo::RBFBox::getAxes, PortType::M_AXES, mimmo::pin::containerTAG::ARR3ARR3, mimmo::pin::dataTAG::FLOAT));
    built = (built && createPortOut<darray3E, RBFBox>(this, &mimmo::RBFBox::getSpan, PortType::M_SPAN, mimmo::pin::containerTAG::ARRAY3, mimmo::pin::dataTAG::FLOAT));

    m_arePortsBuilt = built;
};

/*!Clean all stuffs in your class */
void
RBFBox::clearRBFBox(){
    clear(); //base manipulation stuff clear
    m_origin.fill(0.0);
    m_span.fill(1.0);
    int counter = 0;
    for(auto &val : m_axes)    {
        val.fill(0.0);
        val[counter] = 1.0;
        ++counter;
    }
    m_nodes.clear();
    m_suppR = 0.0;

};

/*! 
 * Return the origin of the RBFBox.
 * \return origin of the box
 */
darray3E
RBFBox::getOrigin(){
    return(m_origin);
}

/*! 
 * Return the span of the RBFBox.
 * \return span of the box
 */
darray3E
RBFBox::getSpan(){
    return(m_span);
}


/*! 
 * Return the oriented axes of the RBFBox.
 * \return axes of the box
 */
dmatrix33E
RBFBox::getAxes(){
    return(m_axes);
}

/*!Set a list of RBF points as control nodes.
 * \param[in] nodes coordinates of control points.
 */
void RBFBox::setNode(dvecarr3E nodes){
    m_nodes = nodes;
    return;
};

/*! Set the value of the support radius R of RBF kernel functions.
 * \param[in] suppR_ new value of support radius.
 */
void
RBFBox::setSupportRadius(double suppR_){
    m_suppR = std::fmax(0.0,suppR_);
}


/*! Plot the OBB as a structured grid to *vtu file.
 * \param[in] directory output directory
 * \param[in] filename  output filename w/out tag
 * \param[in] counter   integer identifier of the file
 * \param[in] binary    boolean flag for 0-"ascii" or 1-"appended" writing
 */
void
RBFBox::plot(std::string directory, std::string filename,int counter, bool binary){


    dvecarr3E activeP(8);

    activeP[0] =  - 0.5 * m_span;
    activeP[6] =    0.5 * m_span;

    activeP[1] = activeP[0]; activeP[1][0] += m_span[0];
    activeP[3] = activeP[0]; activeP[3][1] += m_span[1];
    activeP[2] = activeP[6]; activeP[2][2] += -1.0*m_span[2];

    activeP[7] = activeP[6]; activeP[7][0] += -1.0*m_span[0];
    activeP[5] = activeP[6]; activeP[5][1] += -1.0*m_span[1];
    activeP[4] = activeP[0]; activeP[4][2] += m_span[2];

    darray3E temp;
    dmatrix33E    trasp = bitpit::linearalgebra::transpose(m_axes);
    for(auto &val : activeP){


        for(int i=0; i<3; ++i){
            temp[i] = dotProduct(val, trasp[i]);
        }
        val = temp + m_origin;
    }

    ivector2D activeConn(1);
    for(int i=0; i<8; ++i)    activeConn[0].push_back(i);

    bitpit::VTKFormat codex = bitpit::VTKFormat::ASCII;
    if(binary){codex=bitpit::VTKFormat::APPENDED;}
    bitpit::VTKElementType elDM = bitpit::VTKElementType::HEXAHEDRON;
    bitpit::VTKUnstructuredGrid vtk(directory, filename, elDM);
    vtk.setGeomData( bitpit::VTKUnstructuredField::POINTS, activeP) ;
    vtk.setGeomData( bitpit::VTKUnstructuredField::CONNECTIVITY, activeConn) ;
    vtk.setDimensions(1, 8);
    vtk.setCodex(codex);
    if(counter>=0){vtk.setCounter(counter);}

    vtk.write();
};


/*!Execute your object.
 * It calculates the RBFBox of the input set of RBFs (+1% of support radius).
 *
 */
void
RBFBox::execute(){

    int np = m_nodes.size();

    darray3E min, max;
    min.fill(1.0e+18);
    max.fill(-1.0e+18);

    for(int i=0; i<np; i++){
        for(int j=0; i<3; j++){
            if (m_nodes[i][j] - m_suppR*1.01 < min[j]) min[j] = m_nodes[i][j];
            if (m_nodes[i][j] + m_suppR*1.01 > max[j]) max[j] = m_nodes[i][j];
        }
    }

    m_span = max - min;
    m_origin = min + m_span * 0.5;

};

/*!
 * Plot Optional results of the class, that is the oriented bounding box as *.vtu mesh
 */
void
RBFBox::plotOptionalResults(){

    std::string dir = m_outputPlot ;
    std::string nameGrid  = m_name;
    plot(dir, nameGrid, getClassCounter(), true );
}

/*!
 * It sets infos reading from a XML bitpit::Config::section.
 * \param[in] slotXML bitpit::Config::Section of XML file
 * \param[in] name   name associated to the slot
 */
void
RBFBox::absorbSectionXML(const bitpit::Config::Section & slotXML, std::string name){

    BITPIT_UNUSED(name);
    std::string input;

    if(slotXML.hasOption("Priority")){
        input = slotXML.get("Priority");
        int value =0;
        if(!input.empty()){
            std::stringstream ss(bitpit::utils::trim(input));
            ss>>value;
        }
        setPriority(value);
    };

    if(slotXML.hasOption("SupportRadius")){
        std::string input = slotXML.get("SupportRadius");
        input = bitpit::utils::trim(input);
        double value = 0.0;
        if(!input.empty()){
            std::stringstream ss(input);
            ss >> value;
        }
        setSupportRadius(value);
    }

    if(slotXML.hasOption("PlotInExecution")){
        std::string input = slotXML.get("PlotInExecution");
        input = bitpit::utils::trim(input);
        bool value = false;
        if(!input.empty()){
            std::stringstream ss(input);
            ss >> value;
        }
        setPlotInExecution(value);
    }

    if(slotXML.hasOption("OutputPlot")){
        std::string input = slotXML.get("OutputPlot");
        input = bitpit::utils::trim(input);
        std::string temp = ".";
        if(!input.empty())    setOutputPlot(input);
        else                  setOutputPlot(temp);
    }

}

/*!
 * It sets infos from class members in a XML bitpit::Config::section.
 * \param[in] slotXML bitpit::Config::Section of XML file
 * \param[in] name   name associated to the slot
 */
void
RBFBox::flushSectionXML(bitpit::Config::Section & slotXML, std::string name){

    BITPIT_UNUSED(name);

    slotXML.set("ClassName", m_name);
    slotXML.set("Priority", std::to_string(getPriority()));

    slotXML.set("SupportRadius", std::to_string(m_suppR));

    if(isPlotInExecution()){
        slotXML.set("PlotInExecution", std::to_string(1));
    }
    if(m_outputPlot != "."){
        slotXML.set("OutputPlot", m_outputPlot);
    }

};

}

