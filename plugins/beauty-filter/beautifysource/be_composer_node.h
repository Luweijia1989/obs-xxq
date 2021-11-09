#ifndef BE_COMPOSER_NODE_H
#define BE_COMPOSER_NODE_H

#include <string>


class BEComposerNode {
public:
    BEComposerNode(int majorId, int subId, bool isMajor, std::string nodeName, std::string key, float value = 0.0f);
    int getMajorId() const;
    int getSubId()const;
    const std::string &getNodeName() const;
    const std::string &getKey() const;
    float getValue() const;
    bool isMajor() const {
        return m_isMajor;
    }

    void setMajorId(int id);
    void setSubId(int id);
    void setNodeName(std::string nodeName);
    void setKey(std::string key);
    void setValue(float value);
    void setMajorStatus(bool isMajor);
    
private:
    int m_majorId = -1;
    int m_subId = -1;
    std::string m_nodeName = "";
    std::string m_key = "";
    float m_value = 0.0f;

    bool m_isMajor = true;    
};

#endif // BE_COMPOSER_NODE_H