#pragma once

#include <unordered_map>

#include <bonez/types.hpp>
#include <bonez/scene/lights/LightContainer.hpp>
#include <bonez/scene/sensors/ProjectiveCamera.hpp>

#include <bonez/scene/lights/PointLight.hpp>
#include <bonez/scene/lights/DirectionalLight.hpp>
#include <bonez/scene/lights/EnvironmentLight.hpp>

#include <bonez/sys/time.hpp>

#include "tinyxml/tinyxml2.h"

namespace BnZ {

inline tinyxml2::XMLNode* cloneXMLNode(const tinyxml2::XMLNode* pNode, tinyxml2::XMLDocument* pDocument) {
    if(!pNode) {
        return nullptr;
    }
    auto pClone = pNode->ShallowClone(pDocument);
    for(auto pChild = pNode->FirstChild(); pChild; pChild = pChild->NextSibling()) {
        pClone->InsertEndChild(cloneXMLNode(pChild, pDocument));
    }
    return pClone;
}

template<typename T>
tinyxml2::XMLElement* setChildAttribute(tinyxml2::XMLElement& elt, const char* name, const T& value);

inline bool getAttribute(const tinyxml2::XMLElement& elt, const char* name,
                         float& value) {
    return tinyxml2::XML_NO_ERROR == elt.QueryFloatAttribute(name, &value);
}

inline bool getAttribute(const tinyxml2::XMLElement& elt, const char* name,
                         double& value) {
    return tinyxml2::XML_NO_ERROR == elt.QueryDoubleAttribute(name, &value);
}

inline bool getAttribute(const tinyxml2::XMLElement& elt, const char* name,
                         int& value) {
    return tinyxml2::XML_NO_ERROR == elt.QueryIntAttribute(name, &value);
}

inline bool getAttribute(const tinyxml2::XMLElement& elt, const char* name,
                         uint32_t& value) {
    return tinyxml2::XML_NO_ERROR == elt.QueryUnsignedAttribute(name, &value);
}

inline bool getAttribute(const tinyxml2::XMLElement& elt, const char* name,
                         bool& value) {
    return tinyxml2::XML_NO_ERROR == elt.QueryBoolAttribute(name, &value);
}

inline bool getAttribute(const tinyxml2::XMLElement& elt, const char* name,
                         std::string& value) {
    auto ptr = elt.Attribute(name);
    if(ptr) {
        value = ptr;
        return true;
    }
    return false;
}

template<typename T>
inline bool getAttribute(const tinyxml2::XMLElement& elt, const char* name,
                         T& value) {
    std::string strValue;
    auto r = getAttribute(elt, name, strValue);
    std::stringstream ss;
    ss << strValue;
    ss >> value;
    return r;
}

template<typename T>
inline void setAttribute(tinyxml2::XMLElement& elt, const char* name, const T& value) {
    elt.SetAttribute(name, value);
}

inline void setAttribute(tinyxml2::XMLElement& elt, const char* name,
                         const std::string& value) {
    elt.SetAttribute(name, value.c_str());
}

inline void setAttribute(tinyxml2::XMLElement& elt, const char* name,
                         uint64_t value) {
    setAttribute(elt, name, toString(value));
}

inline void setAttribute(tinyxml2::XMLElement& elt, const char* name,
                         int64_t value) {
    setAttribute(elt, name, toString(value));
}

template<typename T>
inline bool getValue(const tinyxml2::XMLElement& elt, T& value) {
    return getAttribute(elt, "value", value);
}

template<typename T, glm::precision p>
inline bool getValue(const tinyxml2::XMLElement& elt, glm::tvec2<T, p>& value) {
    bool returnValue = true;
    returnValue = returnValue && getAttribute(elt, "x", value.x);
    returnValue = returnValue && getAttribute(elt, "y", value.y);
    return returnValue;
}

template<typename T, glm::precision p>
inline bool getValue(const tinyxml2::XMLElement& elt, glm::tvec3<T, p>& value) {
    bool returnValue = true;
    returnValue = returnValue && getAttribute(elt, "x", value.x);
    returnValue = returnValue && getAttribute(elt, "y", value.y);
    returnValue = returnValue && getAttribute(elt, "z", value.z);
    return returnValue;
}

template<typename T, glm::precision p>
inline bool getValue(const tinyxml2::XMLElement& elt, glm::tvec4<T, p>& value) {
    bool returnValue = true;
    returnValue = returnValue && getAttribute(elt, "x", value.x);
    returnValue = returnValue && getAttribute(elt, "y", value.y);
    returnValue = returnValue && getAttribute(elt, "z", value.z);
    returnValue = returnValue && getAttribute(elt, "w", value.w);
    return returnValue;
}

template<typename T>
inline bool getValue(const tinyxml2::XMLElement& elt, std::vector<T>& values) {
    values.clear();
    auto i = 0u;
    auto pChildElement = elt.FirstChildElement("v0");
    while(pChildElement) {
        T value;
        if(!getValue(*pChildElement, value)) {
            return false;
        }
        values.emplace_back(value);
        ++i;
        pChildElement = pChildElement->NextSiblingElement((std::string("v") + toString(i)).c_str());
    }
    return true;
}

template<typename T>
inline bool getValue(const tinyxml2::XMLElement& elt, std::unordered_map<std::string, T>& values) {
    values.clear();
    for(auto pChildElement = elt.FirstChildElement(); pChildElement; pChildElement = pChildElement->NextSiblingElement()) {
        if(!getValue(*pChildElement, values[pChildElement->Name()])) {
            return false;
        }
    }
    return true;
}

inline bool getValue(const tinyxml2::XMLElement& elt, Microseconds& microseconds) {
    uint64_t count;
    auto pChildElement = elt.FirstChildElement("Microseconds");
    if(pChildElement && getValue(*pChildElement, count)) {
        microseconds = Microseconds { count };
        return true;
    }
    return false;
}

template<typename T>
inline void setValue(tinyxml2::XMLElement& elt, const T& value) {
    setAttribute(elt, "value", value);
}

template<typename T, glm::precision p>
inline void setValue(tinyxml2::XMLElement& elt, const glm::tvec2<T, p>& value) {
    setAttribute(elt, "x", value.x);
    setAttribute(elt, "y", value.y);
}

template<typename T, glm::precision p>
inline void setValue(tinyxml2::XMLElement& elt, const glm::tvec3<T, p>& value) {
    setAttribute(elt, "x", value.x);
    setAttribute(elt, "y", value.y);
    setAttribute(elt, "z", value.z);
}

template<typename T, glm::precision p>
inline void setValue(tinyxml2::XMLElement& elt, const glm::tvec4<T, p>& value) {
    setAttribute(elt, "x", value.x);
    setAttribute(elt, "y", value.y);
    setAttribute(elt, "z", value.z);
    setAttribute(elt, "w", value.w);
}

template<typename T>
inline void setValue(tinyxml2::XMLElement& elt, const std::unordered_map<std::string, T>& values) {
    elt.DeleteChildren();
    for(const auto& pair: values) {
        setChildAttribute(elt, pair.first.c_str(), pair.second);
    }
}

template<typename T>
inline void setValue(tinyxml2::XMLElement& elt, const std::vector<T>& values) {
    elt.DeleteChildren();
    for(const auto& i: range(size(values))) {
        setChildAttribute(elt, (std::string("v") + toString(i)).c_str(), values[i]);
    }
}

inline void setValue(tinyxml2::XMLElement& elt, const Microseconds& microseconds) {
    elt.DeleteChildren();
    setChildAttribute(elt, "Microseconds", microseconds.count());
    setChildAttribute(elt, "Milliseconds", us2ms(microseconds));
    setChildAttribute(elt, "Seconds", us2sec(microseconds));
}

inline void setValue(tinyxml2::XMLElement& elt, const TaskTimer& timer) {
    elt.DeleteChildren();

    auto taskDurations = computeTaskDurations<Microseconds>(timer);
    auto totalTime = std::accumulate(begin(taskDurations), end(taskDurations), Microseconds(0), std::plus<Microseconds>());
    setChildAttribute(elt, "TotalTime", totalTime);
    for(auto taskID: range(timer.getTaskCount())) {
        auto percentage = 100. * taskDurations[taskID].count() / totalTime.count();
        auto pChildElement = setChildAttribute(elt, timer.getTaskName(taskID).c_str(), taskDurations[taskID]);
        setAttribute(*pChildElement, "percentage", percentage);
    }
}

template<typename T>
inline tinyxml2::XMLElement* setChildAttribute(tinyxml2::XMLElement& elt, const char* name, const T& value) {
    auto pChildElement = elt.FirstChildElement(name);
    if(!pChildElement) {
        pChildElement = elt.GetDocument()->NewElement(name);
        elt.InsertEndChild(pChildElement);
    }
    setValue(*pChildElement, value);
    return pChildElement;
}

template<typename T>
inline const tinyxml2::XMLElement* getChildAttribute(const tinyxml2::XMLElement& elt, const char* name, T& value) {
    auto pChildElement = elt.FirstChildElement(name);
    if(pChildElement) {
        getValue(*pChildElement, value);
    }
    return pChildElement;
}

Vec3f loadVector(const tinyxml2::XMLElement& elt);

Vec3f loadColor(const tinyxml2::XMLElement& elt, const Vec3f& defaultColor = zero<Vec3f>());

void storeVector(tinyxml2::XMLElement& elt, const Vec3f& v);

void storeColor(tinyxml2::XMLElement& elt, const Vec3f& c);

PointLight loadPointLight(
        const tinyxml2::XMLElement& elt);

DirectionalLight loadDirectionalLight(
        const tinyxml2::XMLElement& elt);

void loadLights(Scene& scene, const tinyxml2::XMLElement* pLightElt);

void storeLight(
        tinyxml2::XMLElement& elt,
        const PointLight& light);

void storeLight(
        tinyxml2::XMLElement& elt,
        const DirectionalLight& light);

void storeLights(const LightContainer& lightContainer, tinyxml2::XMLElement& lightElt);

void loadPerspectiveCamera(
        const tinyxml2::XMLElement& elt,
        Vec3f& eye, Vec3f& point, Vec3f& up,
        float& fovY);

void storePerspectiveCamera(
        tinyxml2::XMLElement& elt,
        const ProjectiveCamera& camera);

template<typename T>
const tinyxml2::XMLElement* serialize(const tinyxml2::XMLElement& elt, const char* name, T& value) {
    return getChildAttribute(elt, name, value);
}

template<typename T>
tinyxml2::XMLElement* serialize(tinyxml2::XMLElement& elt, const char* name, const T& value) {
    return setChildAttribute(elt, name, value);
}

}
