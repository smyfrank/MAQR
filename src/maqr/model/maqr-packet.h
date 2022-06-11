#ifndef MAQR_PACKET_H
#define MAQR_PACKET_H

#include <iostream>
#include "ns3/header.h"
#include "ns3/ipv4-address.h"
#include "ns3/vector.h"
#include "ns3/enum.h"

namespace ns3 {
namespace maqr {
/**
 * \todo definition of different types of packets
 */
enum MessageType
{
  MAQRTYPE_HELLO = 1,
  MAQRTYPE_ACK = 2,
};

/**
 * \ingroup maqr
 * \brief MAQR types
 */
class TypeHeader : public Header
{
public:
  TypeHeader(MessageType t);

  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId();
  TypeId GetInstanceTypeId() const;
  uint32_t GetSerializedSize() const;
  void Serialize (Buffer::Iterator start) const;
  uint32_t Deserialize(Buffer::Iterator start);
  void Print(std::ostream &os) const;

  /**
   * \returns the type
   */
  MessageType Get() const
  {
    return m_type;
  }
  /**
   * Check that type if valid
   * \returns true if the type is valid
   */
  bool IsValid () const
  {
    return m_valid;
  }
  /**
   * \brief Comparison operator
   * \param o header to compare
   * \return true if the headers are equal
   */
  bool operator== (TypeHeader const & o) const;

private:
  MessageType m_type;
  bool m_valid;
};

/**
 * \brief Stream output operator
 * \param os output stream
 * \return updated stream
 */
std::ostream & operator<< (std::ostream & os, TypeHeader const & h);

/**
 * \todo segment of hello packet
 * \ingroup maqr
 * \brief MAQR HELLO Packet Format
 * \verbatim 
 * 
 * \endverbatim
 */
class HelloHeader : public Header
{
public:
  /**
   * \brief constructor
   * \param reserved8
   * \param reserved16
   * \param origin originator IP address
   * \param qValue q-learning value of originator
   * \param curPos current position
   */
  HelloHeader (uint8_t reserved8 = 0, uint16_t reserved16 = 0, Ipv4Address origin = Ipv4Address(), float qValue = 1.0, Vector2D curPos = Vector2D(0, 0));
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  TypeId GetInstanceTypeId (void) const;
  uint32_t GetSerializedSize () const;
  void Serialize (Buffer::Iterator start) const;
  uint32_t Deserialize (Buffer::Iterator start);
  void Print (std::ostream &os) const;  

  /**
   * \brief Set originator address
   * \param origin the originator address
   */
  void SetOrigin(Ipv4Address origin)
  {
    m_origin = origin;
  }
  /**
   * \brief Get the originator address
   * \returns the originator address
   */
  Ipv4Address GetOrigin() const
  {
    return m_origin;
  }
  /**
   * \brief Set q-value of originator
   * \param v the q value
   */
  void SetqValue(float v)
  {
    m_qValue = v;
  }
  /**
   * \brief get q value of originator
   * \returns the q value
   */
  float GetqValue() const
  {
    return m_qValue;
  }
  /**
   * \brief Set current position
   * \param curPos the current postion
   */
  void SetCurPosition(Vector2D curPos)
  {
    m_curPos = curPos;
  }
  /**
   * \brief Set current position
   * \param x the x coordinate
   * \param y the y coordinate
   */
  void SetCurPosition(double x, double y)
  {
    m_curPos = Vector2D(x, y);
  }
  /**
   * \brief Get current position
   * \returns the current position
   */
  Vector2D GetCurPosition() const
  {
    return m_curPos;
  }
  

private:
  uint8_t m_reserved8;
  uint16_t m_reserved16;
  Ipv4Address m_origin;
  float m_qValue;
  Vector2D m_curPos;
  
};

class AckHeader : public Header
{
public:
  AckHeader (uint8_t reserved8 = 0, uint16_t reserved16 = 0, Ipv4Address origin = Ipv4Address(), 
          Ipv4Address dst = Ipv4Address(), Ipv4Address dataDst = Ipv4Address(), float maxQ = 0, float succSum = 0);

  static TypeId GetTypeId (void);
  TypeId GetInstanceTypeId (void) const;
  uint32_t GetSerializedSize () const;
  void Serialize (Buffer::Iterator start) const;
  uint32_t Deserialize (Buffer::Iterator start);
  void Print (std::ostream &os) const;

    /**
   * \brief Set originator address
   * \param origin the originator address
   */
  void SetOrigin(Ipv4Address origin)
  {
    m_origin = origin;
  }
  /**
   * \brief Get the originator address
   * \returns the originator address
   */
  Ipv4Address GetOrigin() const
  {
    return m_origin;
  }

  void SetDst (Ipv4Address dst)
  {
    m_dst = dst;
  }

  Ipv4Address GetDst () const
  {
    return m_dst;
  }

  void SetDataDst (Ipv4Address dataDst)
  {
    m_dataDst = dataDst;
  }

  Ipv4Address GetDataDst () const
  {
    return m_dataDst;
  }

  void SetMaxQ (float maxQ)
  {
    m_maxQ = maxQ;
  }

  float GetMaxQ () const
  {
    return m_maxQ;
  }

  void SetSuccSum (float succSum)
  {
    m_succSum = succSum;
  }

  float GetSuccSum () const
  {
    return m_succSum;
  }

private:
  uint8_t m_reserved8;
  uint16_t m_reserved16;
  Ipv4Address m_origin;
  Ipv4Address m_dst;
  Ipv4Address m_dataDst;
  float m_maxQ;
  float m_succSum;
};
/**
  * \brief Stream output operator
  * \param os output stream
  * \return updated stream
  */
std::ostream & operator<<(std::ostream & os, HelloHeader const &);

} // namespace maqr
} // namespace ns3

#endif /* MAQR_PACKET_H */