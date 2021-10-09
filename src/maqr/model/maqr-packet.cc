#include "maqr-packet.h"
#include "ns3/address-utils.h"
#include "ns3/packet.h"

/**
 * Helper functions to serialize floating point numbers
 * 
 * Float is stored within an union, and the uint32_t (4-byte) representation of the stored bytes can be gathered.
 * This can then be serialized with the appropriate existing method.
 * 
 */
static uint32_t
FtoU32 (float v)
{
  /**
   * Converts a float to an uint32_t binary representation.
   * \param v The float value
   * \returns The uint32_t binary representation
   */
  union Handler {
    float f;
    uint32_t b;
  };

  Handler h;
  h.f = v;
  return h.b;
}

static float
U32toF (uint32_t v)
{
  /**
   * Converts an uint32_t binary representation to a float.
   * \param v The uint32_t binary representation
   * \returns The float value.
   */
  union Handler {
    float f;
    uint32_t b;
  };

  Handler h;
  h.b = v;
  return h.f;
}

namespace ns3 {
namespace maqr {

NS_OBJECT_ENSURE_REGISTERED(TypeHeader);

TypeHeader::TypeHeader(MessageType t)
  : m_type(t),
    m_valid(true)
{
}

TypeId TypeHeader::GetTypeId()
{
  static TypeId tid = TypeId("ns3::maqr::TypeHeader")
    .SetParent<Header>()
    .SetGroupName("MAQR")
    .AddConstructor<TypeHeader>()
    ;
    return tid;
}

TypeId TypeHeader::GetInstanceTypeId() const
{
  return GetTypeId();
}

uint32_t TypeHeader::GetSerializedSize() const
{
  return 1;
}

void TypeHeader::Serialize(Buffer::Iterator i) const
{
  i.WriteU8((uint8_t)m_type);
}

uint32_t TypeHeader::Deserialize(Buffer::Iterator start)
{
  Buffer::Iterator i = start;
  uint8_t type = i.ReadU8();
  m_valid = true;
  switch(type)
  {
    case MAQRTYPE_HELLO:
    {
      m_type = (MessageType) type;
    }
    default:
      m_valid = false;
  }
  uint32_t dist = i.GetDistanceFrom(start);
  NS_ASSERT(dist == GetSerializedSize());
  return dist;
}

void TypeHeader::Print(std::ostream &os) const
{
  switch(m_type)
  {
    case MAQRTYPE_HELLO:
    {
      os << "HELLO";
      break;
    }
    default:
      os << "UNKOWN_TYPE";
  }
}

bool TypeHeader::operator==(TypeHeader const& o) const
{
  return (m_type == o.m_type && m_valid == o.m_valid);
}

std::ostream& operator<< (std::ostream & os, TypeHeader const & h)
{
  h.Print(os);
  return os;
}

//--------------------------------------------------------------
// HELLO HEADER
//--------------------------------------------------------------
HelloHeader::HelloHeader(uint8_t reserved8, uint16_t reserved16, Ipv4Address origin, float qValue, Vector2D curPos)
  : m_reserved8(reserved8),
    m_reserved16(reserved16),
    m_origin(origin),
    m_qValue(qValue),
    m_curPos(curPos)
{
}

NS_OBJECT_ENSURE_REGISTERED(HelloHeader);

TypeId HelloHeader::GetTypeId()
{
  static TypeId tid = TypeId("ns3::maqr::HelloHeader")
    .SetParent<Header>()
    .SetGroupName("MAQR")
    .AddConstructor<HelloHeader>()
  ;
  return tid;
}

TypeId HelloHeader::GetInstanceTypeId() const
{
  return GetTypeId();
}

uint32_t HelloHeader::GetSerializedSize() const
{
  /**
   * \todo hello header size
   */
  return 19;
}

void HelloHeader::Serialize(Buffer::Iterator i) const
{
  NS_LOG_DEBUG("x pos:" << (float)m_curPos.x << "y pos:" << (float)m_curPos.y);

  i.WriteU8(m_reserved8);
  i.WriteHtonU16(m_reserved16);
  WriteTo(i, m_origin);
  i.WriteHtonU32(FtoU32(m_qValue));
  i.WriteHtonU32(FtoU32((float)m_curPos.x));
  i.WriteHtonU32(FtoU32((float)m_curPos.y));
}

uint32_t HelloHeader::Deserialize(Buffer::Iterator start)
{
  Buffer::Iterator i = start;
  m_reserved8 = i.ReadU8();
  m_reserved16 = i.ReadNtohU16();
  ReadFrom(i, m_origin);
  m_qValue = U32toF(i.ReadNtohU32());
  float curPosX = U32toF(i.ReadNtohU32());
  float curPosY = U32toF(i.ReadNtohU32());
  m_curPos = Vector2D(curPosX, curPosY);

  uint32_t dist = i.GetDistanceFrom(start);
  NS_ASSERT(dist == GetSerializedSize());
  return dist;
}

void HelloHeader::Print(std::ostream & os) const
{
  os << "Originator: " << m_origin << "q-value: " << m_qValue
     << " Current Position: (" << m_curPos.x << ", " << m_curPos.y << ")";

}

std::ostream & operator<< (std::ostream & os, HelloHeader const & h)
{
  h.Print(os);
  return os;
}

}
}