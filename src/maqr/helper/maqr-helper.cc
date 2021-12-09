/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "maqr-helper.h"
#include "ns3/maqr-routing-protocol.h"
#include "ns3/node-list.h"
#include "ns3/names.h"
#include "ns3/ptr.h"
#include "ns3/ipv4-list-routing.h"


namespace ns3 {

MaqrHelper::MaqrHelper () :
  Ipv4RoutingHelper ()
{
  m_agentFactory.SetTypeId ("ns3::maqr::RoutingProtocol");
}

MaqrHelper* MaqrHelper::Copy (void) const
{
  return new MaqrHelper (*this);
}

Ptr<Ipv4RoutingProtocol> MaqrHelper::Create (Ptr<Node> node) const
{
  Ptr<maqr::RoutingProtocol> agent = m_agentFactory.Create<maqr::RoutingProtocol> ();
  node->AggregateObject (agent);
  return agent;
}

void MaqrHelper::Set (std::string name, const AttributeValue &value)
{
  m_agentFactory.Set (name, value);
}

int64_t MaqrHelper::AssignStreams (NodeContainer c, int64_t stream)
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
    Ptr<maqr::RoutingProtocol> maqr = DynamicCast<maqr::RoutingProtocol> (proto);
    if (maqr)
    {
      currentStream += maqr->AssignStreams (currentStream);
      continue;
    }
    // Maqr may also be in a list
    Ptr<Ipv4ListRouting> list = DynamicCast<Ipv4ListRouting> (proto);
    if (list)
    {
      int16_t priority;
      Ptr<Ipv4RoutingProtocol> listProto;
      Ptr<maqr::RoutingProtocol> listMaqr;
      for (uint32_t i = 0; i < list->GetNRoutingProtocols (); ++i)
      {
        listProto = list->GetRoutingProtocol (i, priority);
        listMaqr = DynamicCast<maqr::RoutingProtocol> (listProto);
        if (listMaqr)
        {
          currentStream += listMaqr->AssignStreams (currentStream);
          break;
        }
      }
    }
  }
  return (currentStream - stream);
}

}

