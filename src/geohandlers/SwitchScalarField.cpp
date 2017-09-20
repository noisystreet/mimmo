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
\*---------------------------------------------------------------------------*/

#include "SwitchFields.hpp"
#include "SkdTreeUtils.hpp"
#include "ExtractFields.hpp"

using namespace std;
using namespace bitpit;
namespace mimmo{

/*!
 * Default constructor.
 * \param[in] loc MPVLocation of fields data: POINT, CELL or INTERFACE.
 */
SwitchScalarField::SwitchScalarField(MPVLocation loc):SwitchField(){
    m_name = "mimmo.SwitchScalarField";
    m_loc = loc;
    if(m_loc == MPVLocation::UNDEFINED) m_loc= MPVLocation::POINT;
}

/*!
 * Custom constructor reading xml data
 * \param[in] rootXML reference to your xml tree section
 */
SwitchScalarField::SwitchScalarField(const bitpit::Config::Section & rootXML){

    std::string fallback_name = "ClassNONE";
    std::string fallback_loc  = "-1";
    
    std::string input_name = rootXML.get("ClassName", fallback_name);
    input_name = bitpit::utils::string::trim(input_name);
    
    std::string input_loc = rootXML.get("DataLocation", fallback_loc);
    input_loc = bitpit::utils::string::trim(input_loc);
    
    int loc = std::stoi(input_loc);
    if(loc > 0 && loc < 4){
        m_loc  =static_cast<MPVLocation>(loc);
    }else{
        loc = -1;
    }
    
    m_name = "mimmo.SwitchScalarField";

    if(input_name == "mimmo.SwitchScalarField" && loc != -1){
        absorbSectionXML(rootXML);
    }else{
        warningXML(m_log, m_name);
    };
}

/*!
 * Default destructor
 */
SwitchScalarField::~SwitchScalarField(){
    m_fields.clear();
    m_result.clear();
}

/*!
 * Copy constructor
 */
SwitchScalarField::SwitchScalarField(const SwitchScalarField & other):SwitchField(other){
    m_loc = other.m_loc;
    m_fields = other.m_fields;
    m_result = other.m_result;
}

/*!
 * Swap function.
 * \param[in] x object to be swapped
 */
void SwitchScalarField::swap(SwitchScalarField & x) noexcept
{
    std::swap(m_loc, x.m_loc);
    std::swap(m_fields, x.m_fields);
    std::swap(m_result, x.m_result);
    SwitchField::swap(x);
}

/*!
 * Build the ports of the class;
 */
void
SwitchScalarField::buildPorts(){
    bool built = true;
    built = (built && createPortIn<std::vector<dmpvector1D>, SwitchScalarField>(this, &mimmo::SwitchScalarField::setFields, M_VECSFIELDS, true, 1));
    built = (built && createPortIn<dmpvector1D, SwitchScalarField>(this, &mimmo::SwitchScalarField::addField, M_SCALARFIELD, true, 1));
    built = (built && createPortOut<dmpvector1D, SwitchScalarField>(this, &mimmo::SwitchScalarField::getSwitchedField, M_SCALARFIELD));

    SwitchField::buildPorts();
    m_arePortsBuilt = built;
}

/*!
 * Get switched field.
 * \return switched field
 */
dmpvector1D
SwitchScalarField::getSwitchedField(){
    return m_result;
}

/*!
 * Set list of input fields.
 * \param[in] fields scalar fields
 */
void
SwitchScalarField::setFields(vector<dmpvector1D> fields){
    
    for(auto &ff : fields){
        if(ff.getDataLocation() == m_loc && ff.getGeometry()!= NULL){
            m_fields.push_back(ff);
        }
    }
}

/*!
 * Add a field to the list of input fields.
 * \param[in] field scalar field
 */
void
SwitchScalarField::addField(dmpvector1D field){
    if(field.getDataLocation() == m_loc && field.getGeometry() != NULL){
        m_fields.push_back(field);
    }
    
}

/*!
 * Clear content of the class
 */
void
SwitchScalarField::clear(){
    m_fields.clear();
    m_result.clear();
    SwitchField::clear();
}

/*!
 * Plot switched field on its geometry
 */
void 
SwitchScalarField::plotOptionalResults(){

    if (m_result.size() == 0 || getGeometry() == NULL) return;

    bitpit::VTKLocation loc = bitpit::VTKLocation::UNDEFINED;
    switch(m_loc){
        case MPVLocation::POINT :
            loc = bitpit::VTKLocation::POINT;
            break;
        case MPVLocation::CELL :
            loc = bitpit::VTKLocation::CELL;
            break;
        default:
            (*m_log)<<"Warning: Undefined Reference Location in plotOptionalResults of "<<m_name<<std::endl;
            (*m_log)<<"Interface locations are not supported in VTU writing." <<std::endl;
            break;   
    }
    
    if(loc == bitpit::VTKLocation::UNDEFINED)  return;

    //check size of field and adjust missing values to zero for writing purposes only.
    dmpvector1D field_supp = m_result;
    if(!field_supp.checkDataSizeCoherence()){
        livector1D ids;
        if(loc == bitpit::VTKLocation::POINT)  ids = field_supp.getGeometry()->getVertices().getIds();
        if(loc == bitpit::VTKLocation::CELL)   ids = field_supp.getGeometry()->getCells().getIds();
        for(auto id : ids){
            if(!field_supp.exists(id)){
                field_supp.insert(id,0.0);
            } 
        }
    }
    dvector1D field = field_supp.getDataAsVector();

    bitpit::VTKElementType cellType = getGeometry()->desumeElement();
    liimap mapDataInv;
    dvecarr3E points = getGeometry()->getVertexCoords(&mapDataInv);

    if (cellType == bitpit::VTKElementType::UNDEFINED) return;

    if(cellType != bitpit::VTKElementType::VERTEX){
        ivector2D connectivity = getGeometry()->getCompactConnectivity(mapDataInv);
        bitpit::VTKUnstructuredGrid output(".",m_name+std::to_string(getClassCounter()),cellType);
        output.setGeomData( bitpit::VTKUnstructuredField::POINTS, points);
        output.setGeomData( bitpit::VTKUnstructuredField::CONNECTIVITY, connectivity);
        output.setDimensions(connectivity.size(), points.size());
        output.addData("field", bitpit::VTKFieldType::SCALAR, loc, field);
        output.setCodex(bitpit::VTKFormat::APPENDED);
        output.write();
    }else{
        int size = points.size();
        ivector1D connectivity(size);
        for(int i=0; i<size; ++i)    connectivity[i]=i;
        bitpit::VTKUnstructuredGrid output(".",m_name+std::to_string(getClassCounter()),    cellType);
        output.setGeomData( bitpit::VTKUnstructuredField::POINTS, points);
        output.setGeomData( bitpit::VTKUnstructuredField::CONNECTIVITY, connectivity);
        output.setDimensions(connectivity.size(), points.size());
        output.addData("field", bitpit::VTKFieldType::SCALAR, loc, field);
        output.setCodex(bitpit::VTKFormat::APPENDED);
        output.write();
    }

}


/*!
 * Switch your original field along the input geometry provided
 * \return true if switch without errors
 */
bool
SwitchScalarField::mswitch(){

    if (getGeometry() == NULL) return false;

    m_result.clear();

    //Extract by link to geometry
    for (const auto & field : m_fields){
        if (field.getGeometry() == getGeometry()){
            m_result = field;
            //geometry and location are copied from field.
            return true;
        }
    }

    //Extract by geometric mapping if active and if no positive match is found.
    if (m_mapping){

        m_result.setGeometry(getGeometry());
        m_result.setDataLocation(m_loc);
        
        ExtractScalarField * ef = new ExtractScalarField();
        ef->setGeometry(getGeometry());
        ef->setMode(ExtractMode::MAPPING);
        ef->setTolerance(m_tol);
        
        //create map for overlapping ids purpose;
        std::unordered_map<long, int> idRepetition; 
        
        for (const auto & field : m_fields){
            ef->setField(field);
            ef->execute();
            
            auto temp = ef->getExtractedField();
            auto itB = temp.begin();
            auto itE = temp.end();
            long id;
            for(itB; itB != itE; ++itB){
                id = itB.getId();
                if(!m_result.exists(id)){
                    m_result.insert(id, *itB);
                }else{
                    m_result[id] += *itB;
                    if(idRepetition.count(id)>0){
                        ++idRepetition[id];
                    }else{
                        idRepetition[id] = 2;
                    }
                }
            }
        }
        
        //resolve overlapping ids by averaging correspondent value;
        
        for(auto &itval : idRepetition){
            m_result[itval.first] /= double(itval.second);
        }
        
        delete ef;
    }

    if (m_result.size() == 0) return false;
    return true;
}

/*!
 * It sets infos reading from a XML bitpit::Config::section.
 * \param[in] slotXML bitpit::Config::Section of XML file
 * \param[in] name   name associated to the slot
 */
void SwitchScalarField::absorbSectionXML(const bitpit::Config::Section & slotXML, std::string name){
    
    BITPIT_UNUSED(name);

    if(slotXML.hasOption("DataLocation")){
        std::string input = slotXML.get("DataLocation");
        input = bitpit::utils::string::trim(input);
        int temp = -1;
        if(!input.empty()){
            std::stringstream ss(input);
            ss>>temp;
        }
        if(int(m_loc) != temp){
            (*m_log)<<"Error absorbing DataLocation in "<<m_name<<". Class and read locations mismatch"<<std::endl;
            return;
        }
    }

    SwitchField::absorbSectionXML(slotXML, name);


};

/*!
 * It sets infos from class members in a XML bitpit::Config::section.
 * \param[in] slotXML bitpit::Config::Section of XML file
 * \param[in] name   name associated to the slot
 */
void SwitchScalarField::flushSectionXML(bitpit::Config::Section & slotXML, std::string name){
    
    BITPIT_UNUSED(name);
    slotXML.set("DataLocation", std::to_string(int(m_loc)));
    SwitchField::flushSectionXML(slotXML, name);
    
    if(m_mapping){
        slotXML.set("Mapping", std::to_string(1));
        slotXML.set("Tolerance", std::to_string(m_tol));
    }
    
};


}
