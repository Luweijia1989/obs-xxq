
#include "be_composer_node.h"
#include "be_resource_context.h"

BEComposerNode::BEComposerNode(int majorId, int subId, bool isMajor, std::string nodeName, std::string key, float value)
:m_majorId(majorId), m_subId(subId), m_isMajor(isMajor), m_key(key), m_value(value)
{
    m_nodeName = getResourceContext()->getFeatureContext()->getComposerDir() + nodeName;
}

int BEComposerNode::getMajorId() const {
    return m_majorId;
}

int BEComposerNode::getSubId() const {
    return m_subId;
}

const std::string &BEComposerNode::getNodeName() const {
    return m_nodeName;
}
const std::string &BEComposerNode::getKey() const {
    return m_key;
}
float BEComposerNode::getValue() const {
    return m_value;
}

void BEComposerNode::setMajorId(int id) {
    m_majorId = id;
}

void BEComposerNode::setSubId(int id) {
    m_subId = id;
}

void BEComposerNode::setNodeName(std::string nodeName) {
    m_nodeName = getResourceContext()->getFeatureContext()->getComposerDir() + nodeName;
}

void BEComposerNode::setKey(std::string key) {
    m_key = key;
}
void BEComposerNode::setValue(float value) {
    m_value = value;
}

void BEComposerNode::setMajorStatus(bool isMajor) {
    m_isMajor = isMajor;
}