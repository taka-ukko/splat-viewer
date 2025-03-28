#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>
#include <string>
#include <cstdint>
#include <iostream>
#include <algorithm>
#include <cmath>

#include <miniply.h>

class SplatModel {
public:
    // model data 
    std::vector<float> position;
    std::vector<float> opacity;
    std::vector<float> scale;
    std::vector<float> color;
    std::vector<glm::vec4> colorAndOpacity;
    std::vector<float> rot;
    std::vector<glm::mat4> covAndPos;
    uint32_t numPoints = 0;

    bool flipY;
    bool printToConsole;


    SplatModel(const std::string& plyFile, bool flipY = false, bool printToConsole = false) : 
        printToConsole(printToConsole),
        flipY(flipY)
    {

        // Load the data from the PLY file
        // -------------------------------
        if (!loadPLY(plyFile)) {
            std::cerr << "Failed to load PLY file." << std::endl;
            return;
        }

        // Transform the data to standard units
        // ------------------------------------
        // opacity = sigmoid(read_opacity)
        // scale = exp(read_scale)
        // rot = normalized(rot) for all quaternions
        // color = SH(read_color)
        // ------------------------------------
        transformPoints();


        // Compute the covariance matrix
        // Sigma = R * S * S * R^T
        // where R and S are constructed from the scale and (quaternion) rotation vectors
        computeCovariance();

    };

    ~SplatModel() {};

private:

    bool loadPLY(const std::string& plyFile) {
        if(printToConsole) std::cout << "\nLoading file " << plyFile << std::endl;

        miniply::PLYReader reader(plyFile.c_str());

        if (!reader.valid()) {
            std::cerr << "Failed to open " << plyFile << std::endl;
        return false;
        }

        if (reader.has_element() && reader.element_is("vertex") && reader.load_element()) {
            
            numPoints = reader.num_rows();

            if (printToConsole) std::cout << "Num points: " << numPoints << std::endl;

            { // read x,y,z 
                const uint32_t numElements = 3;
                uint32_t indices[numElements];
                position.resize(numPoints * numElements);
                
                if (reader.find_pos(indices)) {
                    reader.extract_properties(indices, numElements, miniply::PLYPropertyType::Float, position.data());
                    if (printToConsole) std::cout << "Point positions loaded succcesfully" << std::endl;
                } else {
                    std::cerr << "Point positions not found" << std::endl;
                    return false;
                }   
            }
                    
            { // read opacity
                const uint32_t numElements = 1;
                uint32_t indices[numElements];
                opacity.resize(numPoints * numElements);
                
                indices[0] = reader.find_property("opacity");
                
                if(indices[0] == miniply::kInvalidIndex) {
                    std::cerr << "Point opacities not found" << std::endl;
                    return false;
                }

                reader.extract_properties(indices, numElements, miniply::PLYPropertyType::Float, opacity.data());
                if (printToConsole) std::cout << "Point opacities loaded succcesfully" << std::endl;
            }

            { // read scales
                const uint32_t numElements = 3;
                const char* baseName = "scale_";
                uint32_t indices[numElements];
                scale.resize(numPoints * numElements);
                
                
                for (uint32_t i = 0; i < numElements; i++) {
                    std::string name = baseName + std::to_string(i); 
                    indices[i] = reader.find_property(name.c_str()); 
                    if(indices[i] == miniply::kInvalidIndex) {
                        std::cerr << name << " not found" << std::endl;
                        return false;
                    } 
                }
                reader.extract_properties(indices, numElements, miniply::PLYPropertyType::Float, scale.data());
                if (printToConsole) std::cout << "Point scales loaded succcesfully" << std::endl;
            }

            {
                const uint32_t numElements = 4;
                const char* baseName = "rot_";
                uint32_t indices[numElements];
                rot.resize(numPoints * numElements);
                
                
                for (uint32_t i = 0; i < numElements; i++) {
                    std::string name = baseName + std::to_string(i); 
                    indices[i] = reader.find_property(name.c_str()); 
                    if(indices[i] == miniply::kInvalidIndex) {
                        std::cerr << name << " not found" << std::endl;
                        return false;
                    } 
                }
                reader.extract_properties(indices, numElements, miniply::PLYPropertyType::Float, rot.data());
                if (printToConsole) std::cout << "Point rotations loaded succcesfully" << std::endl;
            }

            {
                const uint32_t numElements = 3;
                const char* baseName = "f_dc_";
                uint32_t indices[numElements];
                color.resize(numPoints * numElements);
    
                for (uint32_t i = 0; i < numElements; i++) {
                    std::string name = baseName + std::to_string(i); 
                    indices[i] = reader.find_property(name.c_str()); 
                    if(indices[i] == miniply::kInvalidIndex) {
                        std::cerr << name << " not found" << std::endl;
                        return false;
                    }
                }
                reader.extract_properties(indices, numElements, miniply::PLYPropertyType::Float, color.data());

                for (int i = 0; i < opacity.size(); i++) {
                    colorAndOpacity.push_back(glm::vec4(color[3*i], color[3*i+1], color[3*i+2], opacity[i]));
                }

                if (printToConsole) std::cout << "Point colors loaded succcesfully" << std::endl;
            }

            // if (printToConsole) std::cout << "Ply file loaded succcesfully\n" << std::endl;
            std::cout << "Ply file loaded succcesfully\n" << std::endl;
            return true;

            // TODO include higher SH bands

            // {
            //     const uint32_t numElements1 = 3;
            //     const uint32_t numElements2 = 45;
            //     const char* baseName1 = "f_dc_";
            //     const char* baseName2 = "f_rest_";
            //     uint32_t indices[numElements1 + numElements2];
            //     points->color.resize(points->numPoints * (numElements1 + numElements2));

            //     for (uint32_t i = 0; i < numElements1; i++) {
            //         string name = baseName1 + std::to_string(i); 
            //         indices[i] = reader.find_property(name.c_str()); 
            //         if(indices[i] == miniply::kInvalidIndex) {
            //             std::cerr << name << " not found" << std::endl;
            //             return false;
            //         }
            //     }
            //     for (uint32_t i = 0; i < numElements2; i++) {
            //         string name = baseName2 + std::to_string(i);
            //         const uint32_t totIdx = numElements1 + i; 
            //         indices[totIdx] = reader.find_property(name.c_str()); 
            //         if(indices[totIdx] == miniply::kInvalidIndex) {
            //             std::cerr << name << " not found" << std::endl;
            //             return false;
            //         } 
            //     }
            //     reader.extract_properties(indices, numElements1 + numElements2, miniply::PLYPropertyType::Float, points->color.data());
            //     if (printToConsole) std::cout << "Point colors loaded succcesfully" << std::endl;
            // }

        }

        std::cerr << "Ply file has no element called vertex" << std::endl;

        return false;
    }

    void transformPoints() {

        std::for_each(scale.begin(), scale.end(), [](float &value){ 
            value = std::exp(value); 
        });

        std::for_each(opacity.begin(), opacity.end(), [](float &value){
            value = 1.0f / (1.0 + std::exp(value)); // sigmoid
        });
        
        // TODO normalize quaternions (appears to be normalized by default but never know)
        
        std::for_each(colorAndOpacity.begin(), colorAndOpacity.end(), [](glm::vec4 &vec){
            // 0.282094791773878 = 0.5 * sqrt(1/pi) = Spherical harmonic basis function Y_0^0
            // not sure why need to 0.5f
            vec.x = 0.5f + vec.x * 0.282094791773878f; 
            vec.y = 0.5f + vec.y * 0.282094791773878f; 
            vec.z = 0.5f + vec.z * 0.282094791773878f;
            vec.w = 1.0f / (1.0 + std::exp(vec.w)); // sigmoid
        });

    }

    void computeCovariance() {
        
        // one covariance matrix per point
        covAndPos.resize(numPoints);

        for (int i = 0; i < numPoints; i++) {

            float& scale_x = scale.at(3*i);
            float& scale_y = scale.at(3*i + 1);
            float& scale_z = scale.at(3*i + 2);
            
            glm::mat3 S = glm::mat3(0.0f);
            S[0][0] = scale_x;
            S[1][1] = scale_y;
            S[2][2] = scale_z;
            
            float& rot_r = rot.at(4*i);
            float& rot_i = rot.at(4*i + 1);
            float& rot_j = rot.at(4*i + 2);
            float& rot_k = rot.at(4*i + 3);


            // https://stackoverflow.com/questions/32438252/efficient-way-to-apply-mirror-effect-on-quaternion-rotation
            if (flipY) {
                rot_i = -rot_i;
                rot_k = -rot_k;
            }

            float rot_ri = rot_r * rot_i;
            float rot_rj = rot_r * rot_j;
            float rot_rk = rot_r * rot_k;
            float rot_ii = rot_i * rot_i;
            float rot_ij = rot_i * rot_j;
            float rot_ik = rot_i * rot_k;
            float rot_jj = rot_j * rot_j;
            float rot_jk = rot_j * rot_k;
            float rot_kk = rot_k * rot_k;            
            
            glm::mat3 R = glm::mat3(0.0f);
            R[0][0] = 1.0f - 2.0f * (rot_jj + rot_kk);
            R[0][1] = 2.0f * (rot_ij + rot_rk);
            R[0][2] = 2.0f * (rot_ik - rot_rj);
            
            R[1][0] = 2.0f * (rot_ij - rot_rk);
            R[1][1] = 1.0f - 2.0f * (rot_ii + rot_kk);
            R[1][2] = 2.0f * (rot_jk + rot_ri);
            
            R[2][0] = 2.0f * (rot_ik + rot_rj);
            R[2][1] = 2.0f * (rot_jk + rot_ri);
            R[2][2] = 1.0f - 2.0f * (rot_ii + rot_jj);
            
            glm::mat4 cov = glm::mat4(R * S * S * glm::transpose(R));




            if (flipY) {
                position.at(3*i + 1) =-position.at(3*i + 1); 
            }

            cov[3] = glm::vec4(
                position.at(3*i), 
                position.at(3*i + 1), 
                position.at(3*i + 2), 
                0.0f);

            covAndPos[i] = cov;
        }

    }

    


    
};