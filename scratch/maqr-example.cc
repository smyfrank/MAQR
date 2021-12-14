/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include <iostream>
#include <cmath>
#include "ns3/maqr-module.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-model.h"
#include "ns3/point-to-point-module.h"
#include "ns3/v4ping-helper.h"
#include "ns3/yans-wifi-helper.h"

using namespace ns3;

/**
 * \ingroup maqr-examples
 * \ingroup examples
 * \brief Test script.
 * 
 * This script creates 1-dimensional grid topology and then ping last node from the first one:
 * 
 * [10.0.0.1] <-- step --> [10.0.0.2] <-- step --> [10.0.0.3] <-- step --> [10.0.0.4]
 * 
 * ping 10.0.0.4
 *
 * When 1/3 of simulation time has elapsed, one of the nodes is moved out of
 * range, thereby breaking the topology.  By default, this will result in
 * only 34 of 100 pings being received.  If the step size is reduced
 * to cover the gap, then all pings can be received.
 */
class MaqrExample
{
public:
  MaqrExample ();
  /**
   * \brief Configure script parameters
   * \param argc is the command line argument count
   * \param argv is the command line arguments
   * \return true on successful configuration
   */
  bool Configure (int argc, char **argv);
  // Run simulation
  void Run ();
  /**
   * Report results
   * \param os the output stream
   */
  void Report (std::ostream & os);

private:
  // Number of nodes
  uint32_t size;
  // Distance between nodes, meters
  double step;
  // Simulation time, seconds
  double totalTime;
  // Write per-device PCAP traces if true;
  bool pcap;
  // Print routes if true;
  bool printRoutes;

  // nodes used in the example
  NodeContainer nodes;
  // devices used in the example
  NetDeviceContainer devices;
  // interfaces used in the example
  Ipv4InterfaceContainer interfaces;

private:
  // Create the nodes
  void CreateNodes ();
  // Create the devices
  void CreateDevices ();
  // Create the network
  void InstallInternetStack ();
  // Create the simulation application
  void InstallApplications ();

};

int 
main (int argc, char *argv[])
{
  MaqrExample test;
  if (!test.Configure (argc, argv))
  {
    NS_FATAL_ERROR ("Configuration failed. Aborted.");
  }

  test.Run ();
  test.Report (std::cout);
  return 0;
}

//---------------------------------------------------------------------
MaqrExample::MaqrExample () :
  size (10),
  step (50),
  totalTime (100),
  pcap (true),
  printRoutes (true)
{

}

bool MaqrExample::Configure (int argc, char **argv)
{
  // Enable MAQR logs by default. Comment this if too noisy
  LogComponentEnable("MaqrRoutingProtocol", LOG_LEVEL_ALL);

  SeedManager::SetSeed (12345);
  CommandLine cmd (__FILE__);

  cmd.AddValue ("pcap", "Write PCAP traces.", pcap);
  cmd.AddValue ("printRoutes", "Print routing table dumps.", printRoutes);
  cmd.AddValue ("size", "Number of nodes.", size);
  cmd.AddValue ("time", "Simulation time, s.", totalTime);
  cmd.AddValue ("step", "Grid step, m", step);

  cmd.Parse (argc, argv);
  return true;
}

void MaqrExample::Run ()
{
  //  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue (1)); // enable rts cts all the time.
  CreateNodes ();
  CreateDevices ();
  InstallInternetStack ();
  InstallApplications ();

  std::cout << "Starting simulation for " << totalTime << " s ...\n";

  Simulator::Stop (Seconds (totalTime));
  Simulator::Run ();
  Simulator::Destroy ();
}

void MaqrExample::Report (std::ostream &)
{

}

void MaqrExample::CreateNodes ()
{
  std::cout << "Creating " << (unsigned)size << " nodes " << step << " m apart.\n";
  nodes.Create (size);
  // Name nodes
  for (uint32_t i = 0; i < size; ++i)
  {
    std::ostringstream os;
    os << "node-" << i;
    Names::Add (os.str (), nodes.Get (i));
  }
  // Create static grid
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (step),
                                 "DeltaY", DoubleValue (0),
                                 "GridWidth", UintegerValue (size),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);
}

void MaqrExample::CreateDevices ()
{
  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");
  YansWifiPhyHelper wifiPhy;
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());
  WifiHelper wifi;
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("OfdmRate6Mbps"),
                                "RtsCtsThreshold", UintegerValue (0));
  devices = wifi.Install (wifiPhy, wifiMac, nodes);

  if (pcap)
  {
    wifiPhy.EnablePcapAll (std::string ("maqr"));
  }
}

void MaqrExample::InstallInternetStack ()
{
  MaqrHelper maqr;
  // you can configure MAQR attributes here using maqr.Set(name, value)
  InternetStackHelper stack;
  stack.SetRoutingHelper (maqr);
  stack.Install (nodes);
  Ipv4AddressHelper address;
  address.SetBase ("10.0.0.0", "255.0.0.0");
  interfaces = address.Assign (devices);

  if (printRoutes)
  {
    Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper> ("maqr.routes", std::ios::out);
    maqr.PrintRoutingTableAllAt (Seconds (8), routingStream);
  }
}

void MaqrExample::InstallApplications ()
{
  V4PingHelper ping (interfaces.GetAddress (size - 1));
  ping.SetAttribute ("Verbose", BooleanValue (true));

  ApplicationContainer p = ping.Install (nodes.Get (0));
  p.Start (Seconds (0));
  p.Stop (Seconds (totalTime) - Seconds (0.001));

  // move node away
  Ptr<Node> node = nodes.Get (size / 2);
  Ptr<MobilityModel> mob = node->GetObject<MobilityModel> ();
  Simulator::Schedule (Seconds (totalTime/ 3), &MobilityModel::SetPosition, mob, Vector (1e5, 1e5, 1e5));
}