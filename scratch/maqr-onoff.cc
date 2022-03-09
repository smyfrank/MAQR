#include <fstream>
#include <iostream>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/maqr-module.h"
#include "ns3/aodv-module.h"
#include "ns3/olsr-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/dsr-module.h"
#include "ns3/parrot-module.h"
#include "ns3/saqr-module.h"
#include "ns3/applications-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("maqr-onoff");

class RoutingExperiment
{
public:
  RoutingExperiment ();
  void Run (int nSinks, double txp, std::string CSVfileName);
  std::string CommandSetup (int argc, char **argv);

private:
  Ptr<Socket> SetupPacketReceive (Ipv4Address addr, Ptr<Node> node);
  void ReceivePacket (Ptr<Socket> socket);
  void CheckThroughput ();
  void SetMobilityModel (int nWifis, ns3::NodeContainer &adhocNodes);

  uint32_t port;
  uint32_t bytesTotal;
  uint32_t packetsReceived;

  std::string m_CSVfileName;
  int m_nSinks;
  std::string m_protocolName;
  double m_txp;
  bool m_traceMobility;
  uint32_t m_protocol;
  bool m_pcap;
  uint32_t m_mobilityModel;
};

RoutingExperiment::RoutingExperiment ()
  : port (9),
    bytesTotal (0),
    packetsReceived (0),
    m_CSVfileName ("maqr-onoff.csv"),
    m_traceMobility (false),
    m_protocol (1), // MAQR
    m_pcap (false),
    m_mobilityModel (1)
{
}

static inline std::string
PrintReceivePacket (Ptr<Socket> socket, Ptr<Packet> packet, Address senderAddress)
{
  std::ostringstream oss;
  oss << Simulator::Now ().GetSeconds () << " " << socket->GetNode ()->GetId ();

  if (InetSocketAddress::IsMatchingType (senderAddress))
  {
    InetSocketAddress addr = InetSocketAddress::ConvertFrom (senderAddress);
    oss << " received one packet from " << addr.GetIpv4 ();
  }
  else
  {
    oss << " received one packet!";
  }
  return oss.str ();
}

void RoutingExperiment::ReceivePacket (Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address senderAddress;
  while ((packet = socket->RecvFrom (senderAddress)))
  {
    bytesTotal += packet->GetSize ();
    packetsReceived += 1;
    NS_LOG_UNCOND (PrintReceivePacket (socket, packet, senderAddress));
  }
}

void RoutingExperiment::CheckThroughput ()
{
  double kbs = (bytesTotal * 8.0) / 1000;
  bytesTotal = 0;

  std::ofstream out (m_CSVfileName.c_str (), std::ios::app);

  out << (Simulator::Now ()).GetSeconds () << ","
      << kbs << ","
      << packetsReceived << ","
      << m_nSinks << ","
      << m_protocolName << ","
      << m_txp << ""
      << std::endl;

  out.close ();
  packetsReceived = 0;
  Simulator::Schedule (Seconds (1), &RoutingExperiment::CheckThroughput, this);
}

Ptr<Socket> RoutingExperiment::SetupPacketReceive (Ipv4Address addr, Ptr<Node> node)
{
  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  Ptr<Socket> sink = Socket::CreateSocket (node, tid);
  InetSocketAddress local = InetSocketAddress (addr, port);
  sink->Bind (local);
  sink->SetRecvCallback (MakeCallback (&RoutingExperiment::ReceivePacket, this));

  return sink;
}

std::string RoutingExperiment::CommandSetup (int argc, char **argv)
{
  //LogComponentEnable("MaqrRoutingProtocol", LOG_LEVEL_ALL);

  CommandLine cmd (__FILE__);
  cmd.AddValue ("CSVfileName", "The name of the CSV output file name", m_CSVfileName);
  cmd.AddValue ("traceMobility", "Enable mobility tracing", m_traceMobility);
  cmd.AddValue ("protocol", "1=MAQR;2=AODV;3=OLSR;4=DSDV;5=PARROT;6=SAQR", m_protocol);
  cmd.AddValue ("mobilityModel", "1=ConstantPosition;2=RandomWaypoint", m_mobilityModel);
  cmd.Parse (argc, argv);
  return m_CSVfileName;
}

void RoutingExperiment::SetMobilityModel (int nWifis, ns3::NodeContainer &adhocNodes)
{
  switch (m_mobilityModel)
  {
    case 1:
    {
      MobilityHelper mobility;
      mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                    "MinX", DoubleValue (0.0),
                                    "MinY", DoubleValue (0.0),
                                    "DeltaX", DoubleValue (10),
                                    "DeltaY", DoubleValue (10),
                                    "GridWidth", UintegerValue (10),
                                    "LayoutType", StringValue ("RowFirst"));
      mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
      mobility.Install (adhocNodes);
      break;
    }
    case 2:
    {
      int nodeSpeed = 4; //in m/s
      int nodePause = 0; //in s
      MobilityHelper mobilityAdhoc;
      int64_t streamIndex = 0; // used to get consistent mobility across scenarios

      ObjectFactory pos;
      pos.SetTypeId ("ns3::RandomRectanglePositionAllocator");
      pos.Set ("X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=300.0]"));
      pos.Set ("Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=300.0]"));

      Ptr<PositionAllocator> taPositionAlloc = pos.Create ()->GetObject<PositionAllocator> ();
      streamIndex += taPositionAlloc->AssignStreams (streamIndex);

      std::stringstream ssSpeed;
      ssSpeed << "ns3::UniformRandomVariable[Min=0.0|Max=" << nodeSpeed << "]";
      std::stringstream ssPause;
      ssPause << "ns3::ConstantRandomVariable[Constant=" << nodePause << "]";
      mobilityAdhoc.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
                                      "Speed", StringValue (ssSpeed.str ()),
                                      "Pause", StringValue (ssPause.str ()),
                                      "PositionAllocator", PointerValue (taPositionAlloc));
      mobilityAdhoc.SetPositionAllocator (taPositionAlloc);
      mobilityAdhoc.Install (adhocNodes);
      streamIndex += mobilityAdhoc.AssignStreams (adhocNodes, streamIndex);
      // NS_UNUSED (streamIndex); // From this point, streamIndex is unused
    }
  }
}

int main (int argc, char **argv)
{
  RoutingExperiment experiment;
  std::string CSVfileName = experiment.CommandSetup (argc, argv);

  //blank out the last output file and write the column headers
  std::ofstream out (CSVfileName.c_str ());
  out << "SimulationSecond," <<
  "ReceiveRate," <<
  "PacketsReceived," <<
  "NumberOfSinks," <<
  "RoutingProtocol," <<
  "TransmissionPower" <<
  std::endl;
  out.close ();

  int nSinks = 10;
  double txp = 7.5;

  experiment.Run (nSinks, txp, CSVfileName);
}

void RoutingExperiment::Run (int nSinks, double txp, std::string CSVfileName)
{
  Packet::EnablePrinting ();
  m_nSinks = nSinks;
  m_txp = txp;
  m_CSVfileName = CSVfileName;

  int nWifis = 50;

  double totalTime = 100.0;
  std::string rate ("2048bps");
  std::string phyMode ("DsssRate11Mbps");
  std::string tr_name ("maqr-onoff");
  m_protocolName = "protocol";

  Config::SetDefault ("ns3::OnOffApplication::PacketSize", StringValue ("64"));
  Config::SetDefault ("ns3::OnOffApplication::DataRate", StringValue (rate));

  //Set Non-unicastMode rate to unicast mode
  Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue (phyMode));

  NodeContainer adhocNodes;
  adhocNodes.Create (nWifis);

  // setting up wifi phy and channel using helpers
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy;
  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");
  wifiPhy.SetChannel (wifiChannel.Create ());

  // add a mac and disable rate control
  WifiMacHelper wifiMac;
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue (phyMode),
                                "ControlMode", StringValue (phyMode));
  
  wifiPhy.Set ("TxPowerStart", DoubleValue (txp));
  wifiPhy.Set ("TxPowerEnd", DoubleValue (txp));

  wifiMac.SetType ("ns3::AdhocWifiMac");
  NetDeviceContainer adhocDevices = wifi.Install (wifiPhy, wifiMac, adhocNodes);

  if (m_pcap)
  {
    wifiPhy.EnablePcapAll (std::string ("maqr"));
  }

  SetMobilityModel (nWifis, adhocNodes);

  MaqrHelper maqr;
  AodvHelper aodv;
  OlsrHelper olsr;
  DsdvHelper dsdv;
  PARRoTHelper parrot;
  SaqrHelper saqr;
  DsrHelper dsr;
  Ipv4ListRoutingHelper list;
  InternetStackHelper internet;

  switch (m_protocol)
  {
    case 1:
      list.Add (maqr, 100);
      m_protocolName = "MAQR";
      break;
    case 2:
      list.Add (aodv, 100);
      m_protocolName = "AODV";
      break;
    case 3:
      list.Add (olsr, 100);
      m_protocolName = "OLSR";
      break;
    case 4:
      list.Add (dsdv, 100);
      m_protocolName = "DSDV";
      break;
    case 5:
      list.Add (parrot, 100);
      m_protocolName = "PARROT";
      break;
    case 6:
      list.Add (saqr, 100);
      m_protocolName = "SAQR";
      break;
    default:
      NS_FATAL_ERROR ("No such protocol:" << m_protocol);
  }

  internet.SetRoutingHelper (list);
  internet.Install (adhocNodes);

  NS_LOG_INFO ("assigning ip address");

  Ipv4AddressHelper addressAdhoc;
  addressAdhoc.SetBase ("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer adhocInterfaces;
  adhocInterfaces = addressAdhoc.Assign (adhocDevices);

  OnOffHelper onoff1 ("ns3::UdpSocketFactory", Address ());
  onoff1.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"));
  onoff1.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"));

  for (int i = 0; i < nSinks; ++i)
  {
    Ptr<Socket> sink = SetupPacketReceive (adhocInterfaces.GetAddress (i), adhocNodes.Get (i));

    AddressValue remoteAddress (InetSocketAddress (adhocInterfaces.GetAddress (i), port));
    onoff1.SetAttribute ("Remote", remoteAddress);

    Ptr<UniformRandomVariable> var = CreateObject<UniformRandomVariable> ();
    ApplicationContainer temp = onoff1.Install (adhocNodes.Get (i + nSinks));
    temp.Start (Seconds (var->GetValue (10.0, 11.0)));
    temp.Stop (Seconds (totalTime));
  }

  //NS_LOG_INFO ("Configure Tracing.");
  //tr_name = tr_name + "_" + m_protocolName +"_" + nodes + "nodes_" + sNodeSpeed + "speed_" + sNodePause + "pause_" + sRate + "rate";

  //AsciiTraceHelper ascii;
  //Ptr<OutputStreamWrapper> osw = ascii.CreateFileStream ( (tr_name + ".tr").c_str());
  //wifiPhy.EnableAsciiAll (osw);
  AsciiTraceHelper ascii;
  // MobilityHelper::EnableAsciiAll (ascii.CreateFileStream (m_CSVfileName + ".mob"));

  Ptr<FlowMonitor> flowmon;
  FlowMonitorHelper flowmonHelper;
  flowmon = flowmonHelper.InstallAll ();


  NS_LOG_INFO ("Run Simulation.");

  CheckThroughput ();

  Simulator::Stop (Seconds (totalTime));
  Simulator::Run ();

  flowmon->SerializeToXmlFile ((m_CSVfileName + ".xml").c_str(), true, true);

  Simulator::Destroy ();
}