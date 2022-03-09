/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "saqr-helper.h"
#include "ns3/saqr-routing-protocol.h"
#include "ns3/node-list.h"
#include "ns3/names.h"
#include "ns3/ptr.h"
#include "ns3/ipv4-list-routing.h"


namespace ns3 {

SaqrHelper::SaqrHelper () :
  Ipv4RoutingHelper ()
{
  m_agentFactory.SetTypeId ("ns3::saqr::RoutingProtocol");
}

SaqrHelper* SaqrHelper::Copy (void) const
{
  return new SaqrHelper (*this);
}

Ptr<Ipv4RoutingProtocol> SaqrHelper::Create (Ptr<Node> node) const
{
  Ptr<saqr::RoutingProtocol> agent = m_agentFactory.Create<saqr::RoutingProtocol> ();
  node->AggregateObject (agent);
  return agent;
}

void SaqrHelper::Set (std::string name, const AttributeValue &value)
{
  m_agentFactory.Set (name, value);
}

int64_t SaqrHelper::AssignStreams (NodeContainer c, int64_t stream)
{
  int64_t currentStream = stream;
  Ptr<Node> node;
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
  {
    node = (*i);
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
    NS_ASSERT_MSG (ipv4, "Ipv4 not installed on node");
    Ptr<Ipv4RoutingProtocol> proto = ipv4->GetRoutingProtocol ();
    NS_ASSERT_MSG (proto, "Ipv4 routing node installed on node");
    Ptr<saqr::RoutingProtocol> saqr = DynamicCast<saqr::RoutingProtocol> (proto);
    if (saqr)
    {
      currentStream += saqr->AssignStreams (currentStream);
      continue;
    }
    // Saqr may also be in a list
    Ptr<Ipv4ListRouting> list = DynamicCast<Ipv4ListRouting> (proto);
    if (list)
    {
      int16_t priority;
      Ptr<Ipv4RoutingProtocol> listProto;
      Ptr<saqr::RoutingProtocol> listSaqr;
      for (uint32_t i = 0; i < list->GetNRoutingProtocols (); ++i)
      {
        listProto = list->GetRoutingProtocol (i, priority);
        listSaqr = DynamicCast<saqr::RoutingProtocol> (listProto);
        if (listSaqr)
        {
          currentStream += listSaqr->AssignStreams (currentStream);
          break;
        }
      }
    }
  }
  return (currentStream - stream);
}

}

