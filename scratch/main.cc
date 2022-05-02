/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include <fstream>
#include "ns3/core-module.h"
#include "ns3/sixlowpan-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/mobility-model.h"
#include "ns3/mobility-helper.h"
#include "ns3/network-module.h"
//#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include <yaml-cpp/yaml.h>

using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE("TerceiroExemplo");

// void SchedAppArrival(Ptr<ControlLayerClientHelper> controller, std::string policy, float start, float duration, float cpu, float memory, float storage, int app_id, NodeContainer controlNodes, Ipv6InterfaceContainer controlInterfaces, bool balanced)
// {
//   (*controller).AddAppToDatabase(policy, start, duration, cpu, memory, storage);
//   (*controller).AllocateAPP(app_id, controlNodes, controlInterfaces, balanced);
// }

int main(int argc, char *argv[])
{
  bool verbose = false;
  bool balanced = false;
  uint32_t seed = 42;

  CommandLine cmd(__FILE__);
  cmd.AddValue("verbose", "Tell control applications to log if true", verbose);
  cmd.AddValue("balanced", "Tell control whether is a balanced policy", balanced);
  cmd.AddValue("seed", "Set seed as an input parameter", seed);

  cmd.Parse(argc, argv);

  SeedManager::SetSeed(seed);

  if (verbose)
  {
    LogComponentEnable("ControlLayerClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("ControlLayerServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
  }

  YAML::Node input = YAML::LoadFile("./scratch/input.yaml");
  YAML::Node nodes = input["nodes"];
  YAML::Node apps = input["applications"];

  u_int32_t simulationTime = input["configs"][1]["simulationTime"].as<int>();

  NodeContainer controlNodes;
  controlNodes.Create(1);
  Names::Add("Controller", controlNodes.Get(0));

  NodeContainer workerNodes;

  for (auto attr : nodes)
  {
    int nodeQtd = attr["nNodes"].as<int>();
    if (nodeQtd > 0)
    {
      // Definir os Atributos
      Config::SetDefault("ns3::Node::Power", DoubleValue(attr["power"].as<double>()));
      Config::SetDefault("ns3::Node::InitialConsumption", DoubleValue(attr["initialConsumption"].as<double>()));
      Config::SetDefault("ns3::Node::CurrentConsumption", DoubleValue(attr["currentConsumption"].as<double>()));
      Config::SetDefault("ns3::Node::CPU", DoubleValue(attr["cpu"].as<double>()));
      Config::SetDefault("ns3::Node::Memory", DoubleValue(attr["memory"].as<double>()));
      Config::SetDefault("ns3::Node::Transmission", DoubleValue(attr["transmission"].as<double>()));
      Config::SetDefault("ns3::Node::Storage", DoubleValue(attr["storage"].as<double>()));

      // Criar os workers
      workerNodes.Create(nodeQtd);

      // Nomear os workers
      for (int i = 0; i < nodeQtd; i++)
      {
        int nodeId = workerNodes.GetN() - 1 - i;
        string auxWorkerID = attr["name"].as<string>() + to_string(nodeQtd - i);
        Names::Add(auxWorkerID, workerNodes.Get(nodeId));
      }
    }
  }
  controlNodes.Add(workerNodes);

  // cout << "List of nodes:" << endl;
  // for (NodeList::Iterator node = NodeList::Begin(); node != NodeList::End(); node++)
  // {
  //   cout << "Noh: " << (*node) << "; noh ID: " << (*node)->GetId() << "; Nome do noh: " << Names::FindName(*node) << ";" << endl;
  // }
  // cout << "" << endl;

  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

  Ptr<ListPositionAllocator> nodesPositionAlloc = CreateObject<ListPositionAllocator>();
  nodesPositionAlloc->Add(Vector(0.0, 0.0, 0.0));
  nodesPositionAlloc->Add(Vector(50.0, 0.0, 0.0));
  mobility.SetPositionAllocator(nodesPositionAlloc);
  mobility.Install(controlNodes);

  LrWpanHelper ethernet;
  //lrWpanHelper.EnableLogComponents();
  NetDeviceContainer controlDevices = ethernet.Install(controlNodes);
  ethernet.AssociateToPan(controlDevices, 10);

  InternetStackHelper stack;
  stack.SetIpv4StackInstall(false);
  stack.Install(controlNodes);

  SixLowPanHelper sixlowpan;
  NetDeviceContainer sixlpDevices = sixlowpan.Install(controlDevices);

  Ipv6AddressHelper address;
  address.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer controlInterfaces = address.Assign(sixlpDevices);

  ControlLayerServerHelper controlServer(9);
  UdpEchoServerHelper workerServer(7);

  ApplicationContainer controlApps = controlServer.Install(controlNodes);
  controlApps.Start(Seconds(1.0));
  controlApps.Stop(Seconds(simulationTime));

  ApplicationContainer workerApps = workerServer.Install(workerNodes);
  workerApps.Start(Seconds(1.0));
  workerApps.Stop(Seconds(simulationTime));

  ControlLayerClientHelper controller(Ipv6Address("2001:1::2"), 9);
  ApplicationContainer controlClient = controller.Install("/Names/Controller");
  controlClient.Start(Seconds(1.0));
  controlClient.Stop(Seconds(simulationTime));

  controller.DropDatabase();
  controller.CreateDatabase();
  controller.initialize(controlNodes, controlInterfaces, balanced);


  cout << "List of nodes:" << endl;
  for (NodeList::Iterator node = NodeList::Begin(); node != NodeList::End(); node++)
  {
    cout << "Node ID: " << (*node)->GetId() << "; Address: " << controlInterfaces.GetAddress ((*node)->GetId(), 0) << "; Name: " << Names::FindName(*node) << ";" << endl;
  }
  cout << "" << endl;

  // add nodes to database
  // PAULO: O QUE PODERIAMOS FAZER DEPOIS AQUI EH MANDAR UM PKT PARA CADA NOH
  // SOLICITANDO AS INFORMACOES PARA CADASTRAR NO BANCO
  for (uint32_t i = 0; i < workerNodes.GetN(); i++)
  {
    Ptr<Node> node = workerNodes.Get(i);

    controller.AddNodeToDatabase(
        node->GetPower(),
        node->GetInitialConsumption(),
        node->GetCurrentConsumption(),
        node->GetCpu(),
        node->GetMemory(),
        node->GetTransmission(),
        node->GetStorage(),
        Names::FindName(node));
  }

  //add applications to database
  for (std::size_t i = 0; i < apps.size(); i++)
  {

    std::string policy = apps[i]["policy"].as<std::string>();
    float start = apps[i]["start"].as<float>();
    float duration = apps[i]["duration"].as<float>();
    float cpu = apps[i]["cpu"].as<float>();
    float memory = apps[i]["memory"].as<float>();
    float storage = apps[i]["storage"].as<float>();

    int app_id = i + 1;

    // controller.AddAppToDatabase(policy, start, duration, cpu, memory, storage);
//    void (ControlLayerClientHelper::*AddToDB)(std::string, float, float, double, double, double) = &ControlLayerClientHelper::AddAppToDatabase;
    Simulator::Schedule(Seconds(start), &ControlLayerClientHelper::AddAppToDatabase, &controller, policy, start, duration, cpu, memory, storage);

    // controller.AllocateAPP(app_id, controlNodes, controlInterfaces, balanced);
//    void (ControlLayerClientHelper::*AllPP)(int) = &ControlLayerClientHelper::AllocateAPP;
    Simulator::Schedule(Seconds(start), &ControlLayerClientHelper::AllocateAPP, &controller, app_id);
  }

  // controller.Manager(apps.size());

  // std::string scenarioName = input["configs"][0]["scenarioName"].as<std::string>();

  // ethernet.EnablePcap ("terceiro", controlDevices.Get (1), true);
  //ethernet.EnablePcapAll (scenarioName);

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}
