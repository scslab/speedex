import json
import sys

string = sys.stdin.read()

res = json.loads(string)

def get_name(tags):
    for obj in tags:
        key = obj["Key"]
        if key == "Name":
            return obj["Value"]

    raise Error("invalid key")

hosts = {}

for reservation in res["Reservations"]:
    instances = reservation["Instances"]
    
    for instance in instances:
        if (instance["State"]["Name"] != "running"):
            continue
        name = get_name(instance["Tags"])
        print(name)
        ip = instance["PrivateIpAddress"]
        pub = instance["PublicDnsName"]
        hosts[name] = {}
        hosts[name]["ip"] = ip
        hosts[name]["pub"] = pub
        hosts[name]["id"] = instance["InstanceId"]

print (hosts)

with open("awsinv", 'w') as f:
        f.write ("[host0]\n")
        if (len(hosts["node-0"]["pub"]) > 0):
            f.write("ubuntu@")
            f.write(hosts["node-0"]["pub"])
        f.write  ("\n[host1]\n")
        if (len(hosts["node-1"]["pub"]) > 0):
            f.write("ubuntu@")
            f.write(hosts["node-1"]["pub"])
        f.write  ("\n[host2]\n")
        if (len(hosts["node-2"]["pub"]) > 0):
            f.write("ubuntu@")
            f.write(hosts["node-2"]["pub"])
        f.write  ("\n[host3]\n")
        if (len(hosts["node-3"]["pub"]) > 0):
            f.write("ubuntu@")
            f.write(hosts["node-3"]["pub"])
        f.write  ("\n") 

with open("config/config_aws_generated.yaml", 'w') as f:
    f.write("num_replicas: 4\n\n")
    f.write("replica_0:\n  sk_seed: 1\n  hostname: \"")
    f.write(hosts["node-0"]["ip"])
    f.write("\"\n\n")
    f.write("replica_1:\n  sk_seed: 2\n  hostname: \"")
    f.write(hosts["node-1"]["ip"])
    f.write("\"\n\n")
    f.write("replica_2:\n  sk_seed: 3\n  hostname: \"")
    f.write(hosts["node-2"]["ip"])
    f.write("\"\n\n")
    f.write("replica_3:\n  sk_seed: 4\n  hostname: \"")
    f.write(hosts["node-3"]["ip"])
    f.write("\"\n\n")

