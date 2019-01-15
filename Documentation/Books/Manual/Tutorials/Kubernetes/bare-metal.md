<!-- don't edit here, it's from https://@github.com/arangodb/kube-arangodb.git / docs/Manual/ -->
# ArangoDB on bare metal Kubernetes

A not of warning for lack of a better word upfront: Kubernetes is
awesome and powerful. As with awesome and powerful things, there is
infinite ways of setting up a k8s cluster. With great flexibility
comes great complexity. There are inifinite ways of hitting barriers.

This guide is a walk through for, again in lack of a better word,
a reasonable and flexibel setup to get to an ArangoDB cluster setup on
a baremetal kubernetes setup.

## BEWARE: Do not use this setup for production!

This guide does not involve setting up dedicated master nodes or high availability for Kubernetes, but uses for sake of simplicity a single untainted master. This is the very definition of a test environment.

If you are interested in running a high available Kubernetes setup, please refer to: [Creating Highly Available Clusters with kubeadm](https://kubernetes.io/docs/setup/independent/high-availability/)

## Requirements

Let there be 3 Linux boxes, `kube01 (192.168.10.61)`, `kube02 (192.168.10.62)` and `kube03 (192.168.10.3)`, with `kubeadm` and `kubectl` installed and off we go:

* `kubeadm`, `kubectl` version `>=1.10` 

## Initialise the master node

The master node is outstanding in that it handles the API server and some other vital infrastructure 

```
sudo kubeadm init --pod-network-cidr=10.244.0.0/16
```

```
  [init] Using Kubernetes version: v1.13.2
  [preflight] Running pre-flight checks
  [preflight] Pulling images required for setting up a Kubernetes cluster
  [preflight] This might take a minute or two, depending on the speed of your internet connection
  [preflight] You can also perform this action in beforehand using 'kubeadm config images pull'
  [kubelet-start] Writing kubelet environment file with flags to file "/var/lib/kubelet/kubeadm-flags.env"
  [kubelet-start] Writing kubelet configuration to file "/var/lib/kubelet/config.yaml"
  [kubelet-start] Activating the kubelet service
  [certs] Using certificateDir folder "/etc/kubernetes/pki"
  [certs] Generating "ca" certificate and key
  [certs] Generating "apiserver" certificate and key
  [certs] apiserver serving cert is signed for DNS names [kube01 kubernetes kubernetes.default kubernetes.default.svc kubernetes.default.svc.cluster.local] and IPs [10.96.0.1 192.168.10.61]
  [certs] Generating "apiserver-kubelet-client" certificate and key
  [certs] Generating "front-proxy-ca" certificate and key
  [certs] Generating "front-proxy-client" certificate and key
  [certs] Generating "etcd/ca" certificate and key
  [certs] Generating "apiserver-etcd-client" certificate and key
  [certs] Generating "etcd/server" certificate and key
  [certs] etcd/server serving cert is signed for DNS names [kube01 localhost] and IPs [192.168.10.61 127.0.0.1 ::1]
  [certs] Generating "etcd/peer" certificate and key
  [certs] etcd/peer serving cert is signed for DNS names [kube01 localhost] and IPs [192.168.10.61 127.0.0.1 ::1]
  [certs] Generating "etcd/healthcheck-client" certificate and key
  [certs] Generating "sa" key and public key
  [kubeconfig] Using kubeconfig folder "/etc/kubernetes"
  [kubeconfig] Writing "admin.conf" kubeconfig file
  [kubeconfig] Writing "kubelet.conf" kubeconfig file
  [kubeconfig] Writing "controller-manager.conf" kubeconfig file
  [kubeconfig] Writing "scheduler.conf" kubeconfig file
  [control-plane] Using manifest folder "/etc/kubernetes/manifests"
  [control-plane] Creating static Pod manifest for "kube-apiserver"
  [control-plane] Creating static Pod manifest for "kube-controller-manager"
  [control-plane] Creating static Pod manifest for "kube-scheduler"
  [etcd] Creating static Pod manifest for local etcd in "/etc/kubernetes/manifests"
  [wait-control-plane] Waiting for the kubelet to boot up the control plane as static Pods from directory "/etc/kubernetes/manifests". This can take up to 4m0s
  [apiclient] All control plane components are healthy after 23.512869 seconds
  [uploadconfig] storing the configuration used in ConfigMap "kubeadm-config" in the "kube-system" Namespace
  [kubelet] Creating a ConfigMap "kubelet-config-1.13" in namespace kube-system with the configuration for the kubelets in the cluster
  [patchnode] Uploading the CRI Socket information "/var/run/dockershim.sock" to the Node API object "kube01" as an annotation
  [mark-control-plane] Marking the node kube01 as control-plane by adding the label "node-role.kubernetes.io/master=''"
  [mark-control-plane] Marking the node kube01 as control-plane by adding the taints [node-role.kubernetes.io/master:NoSchedule]
  [bootstrap-token] Using token: blcr1y.49wloegyaugice8a
  [bootstrap-token] Configuring bootstrap tokens, cluster-info ConfigMap, RBAC Roles
  [bootstraptoken] configured RBAC rules to allow Node Bootstrap tokens to post CSRs in order for nodes to get long term certificate credentials
  [bootstraptoken] configured RBAC rules to allow the csrapprover controller automatically approve CSRs from a Node Bootstrap Token
  [bootstraptoken] configured RBAC rules to allow certificate rotation for all node client certificates in the cluster
  [bootstraptoken] creating the "cluster-info" ConfigMap in the "kube-public" namespace
  [addons] Applied essential addon: CoreDNS
  [addons] Applied essential addon: kube-proxy

  Your Kubernetes master has initialized successfully!

  To start using your cluster, you need to run the following as a regular user:

    mkdir -p $HOME/.kube
    sudo cp -i /etc/kubernetes/admin.conf $HOME/.kube/config
    sudo chown $(id -u):$(id -g) $HOME/.kube/config

  You should now deploy a pod network to the cluster.
  Run "kubectl apply -f [podnetwork].yaml" with one of the options listed at:
    https://kubernetes.io/docs/concepts/cluster-administration/addons/

  You can now join any number of machines by running the following on each node as root:

  kubeadm join 192.168.10.61:6443 --token blcr1y.49wloegyaugice8a --discovery-token-ca-cert-hash sha256:0505933664d28054a62298c68dc91e9b2b5cf01ecfa2228f3c8fa2412b7a78c8
```

Go ahead and do as above instructed and see into getting kubectl to work on the master:

```
mkdir -p $HOME/.kube
sudo cp -i /etc/kubernetes/admin.conf $HOME/.kube/config
sudo chown $(id -u):$(id -g) $HOME/.kube/config
```

## Deploy a pod network

For this guide, we go with **flannel**, as it is an easy way of setting up a layer 3 network, which uses the Kubernetes API and just works anywhere, where a network between the involved machines works:

```
kubectl apply -f \ 
   https://raw.githubusercontent.com/coreos/flannel/bc79dd1505b0c8681ece4de4c0d86c5cd2643275/Documentation/kube-flannel.yml
```
```
  clusterrole.rbac.authorization.k8s.io/flannel created
  clusterrolebinding.rbac.authorization.k8s.io/flannel created
  serviceaccount/flannel created
  configmap/kube-flannel-cfg created
  daemonset.extensions/kube-flannel-ds-amd64 created
  daemonset.extensions/kube-flannel-ds-arm64 created
  daemonset.extensions/kube-flannel-ds-arm created
  daemonset.extensions/kube-flannel-ds-ppc64le created
  daemonset.extensions/kube-flannel-ds-s390x created
```

## Join remaining nodes

Run the above join commands on the nodes `kube02` and `kube03`. Below is the output on `kube02` for the setup for this guide:

```
sudo kubeadm join 192.168.10.61:6443 --token blcr1y.49wloegyaugice8a --discovery-token-ca-cert-hash sha256:0505933664d28054a62298c68dc91e9b2b5cf01ecfa2228f3c8fa2412b7a78c8
```
```
  [preflight] Running pre-flight checks
  [discovery] Trying to connect to API Server "192.168.10.61:6443"
  [discovery] Created cluster-info discovery client, requesting info from "https:// 192.168.10.61:6443"
  [discovery] Requesting info from "https://192.168.10.61:6443" again to validate TLS against the pinned public key
  [discovery] Cluster info signature and contents are valid and TLS certificate validates against pinned roots, will use API Server "192.168.10.61:6443"
  [discovery] Successfully established connection with API Server "192.168.10.61:6443"
  [join] Reading configuration from the cluster...
  [join] FYI: You can look at this config file with 'kubectl -n kube-system get cm kubeadm-config -oyaml'
  [kubelet] Downloading configuration for the kubelet from the "kubelet-config-1.13" ConfigMap in the kube-system namespace
  [kubelet-start] Writing kubelet configuration to file "/var/lib/kubelet/config.yaml"
  [kubelet-start] Writing kubelet environment file with flags to file "/var/lib/kubelet/kubeadm-flags.env"
  [kubelet-start] Activating the kubelet service
  [tlsbootstrap] Waiting for the kubelet to perform the TLS Bootstrap...
  [patchnode] Uploading the CRI Socket information "/var/run/dockershim.sock" to the Node API object "kube02" as an annotation

This node has joined the cluster:
* Certificate signing request was sent to apiserver and a response was received.
* The Kubelet was informed of the new secure connection details.

Run 'kubectl get nodes' on the master to see this node join the cluster.
```

## Untaint master node

```
kubectl taint nodes --all node-role.kubernetes.io/master-
```
```
  node/kube01 untainted
  taint "node-role.kubernetes.io/master:" not found
  taint "node-role.kubernetes.io/master:" not found
```

## Wait for nodes to get ready and sanity checking

After some brief period, you should see that your nodes are good to go:

```
kubectl get nodes
```
```
  NAME     STATUS   ROLES    AGE   VERSION
  kube01   Ready    master   38m   v1.13.2
  kube02   Ready    <none>   13m   v1.13.2
  kube03   Ready    <none>   63s   v1.13.2
```

Just a quick sanity check to see, that your cluster is up and running:

```
kubectl get all --all-namespaces
```
```
  NAMESPACE     NAME                                 READY   STATUS    RESTARTS   AGE
  kube-system   pod/coredns-86c58d9df4-r9l5c         1/1     Running   2          41m
  kube-system   pod/coredns-86c58d9df4-swzpx         1/1     Running   2          41m
  kube-system   pod/etcd-kube01                      1/1     Running   2          40m
  kube-system   pod/kube-apiserver-kube01            1/1     Running   2          40m
  kube-system   pod/kube-controller-manager-kube01   1/1     Running   2          40m
  kube-system   pod/kube-flannel-ds-amd64-hppt4      1/1     Running   3          16m
  kube-system   pod/kube-flannel-ds-amd64-kt6jh      1/1     Running   1          3m41s
  kube-system   pod/kube-flannel-ds-amd64-tg7gz      1/1     Running   2          20m
  kube-system   pod/kube-proxy-f2g2q                 1/1     Running   2          41m
  kube-system   pod/kube-proxy-gt9hh                 1/1     Running   0          3m41s
  kube-system   pod/kube-proxy-jwmq7                 1/1     Running   2          16m
  kube-system   pod/kube-scheduler-kube01            1/1     Running   2          40m

  NAMESPACE     NAME                 TYPE        CLUSTER-IP   EXTERNAL-IP   PORT(S)         AGE
  default       service/kubernetes   ClusterIP   10.96.0.1    <none>        443/TCP         41m
  kube-system   service/kube-dns     ClusterIP   10.96.0.10   <none>        53/UDP,53/TCP   41m
```

## Deploy helm

- Obtain current [helm release](https://github.com/helm/helm/releases) for your architecture

- Create tiller user

  ```
  kubectl create serviceaccount --namespace kube-system tiller
  ```
  ```
    serviceaccount/tiller created
  ```

- Attach `tiller` to proper role

  ```
  kubectl create clusterrolebinding tiller-cluster-rule \
    --clusterrole=cluster-admin --serviceaccount=kube-system:tiller
  ```
  ```
    clusterrolebinding.rbac.authorization.k8s.io/tiller-cluster-rule created
  ```

- Initialise helm

  ```
  helm init --service-account tiller
  ```
  ```
    $HELM_HOME has been configured at /home/xxx/.helm.
    ...
    Happy Helming!

    Tiller (the Helm server-side component) has been
    installed into your Kubernetes Cluster.
  ```

## Deploy ArangoDB operator charts

- Deploy ArangoDB custom resource definition chart

```
helm install https://github.com/arangodb/kube-arangodb/releases/download/0.3.7/kube-arangodb-crd.tgz
```
```
  NAME:   hoping-gorilla
  LAST DEPLOYED: Mon Jan 14 06:10:27 2019
  NAMESPACE: default
  STATUS: DEPLOYED

  RESOURCES:
  ==> v1beta1/CustomResourceDefinition
  NAME                                                            AGE
  arangodeployments.database.arangodb.com                         0s
  arangodeploymentreplications.replication.database.arangodb.com  0s


  NOTES:

  kube-arangodb-crd has been deployed successfully!

  Your release is named 'hoping-gorilla'.

  You can now continue install kube-arangodb chart.
```
- Deploy ArangoDB operator chart

```
helm install https://github.com/arangodb/kube-arangodb/releases/download/0.3.7/kube-arangodb.tgz
```
```
  NAME:   illocutionary-whippet
  LAST DEPLOYED: Mon Jan 14 06:11:58 2019
  NAMESPACE: default
  STATUS: DEPLOYED

  RESOURCES:
  ==> v1beta1/ClusterRole
  NAME                                                   AGE
  illocutionary-whippet-deployment-replications          0s
  illocutionary-whippet-deployment-replication-operator  0s
  illocutionary-whippet-deployments                      0s
  illocutionary-whippet-deployment-operator              0s

  ==> v1beta1/ClusterRoleBinding
  NAME                                                           AGE
  illocutionary-whippet-deployment-replication-operator-default  0s
  illocutionary-whippet-deployment-operator-default              0s

  ==> v1beta1/RoleBinding
  NAME                                           AGE
  illocutionary-whippet-deployment-replications  0s
  illocutionary-whippet-deployments              0s

  ==> v1/Service
  NAME                                    TYPE       CLUSTER-IP     EXTERNAL-IP  PORT(S)    AGE
  arango-deployment-replication-operator  ClusterIP  10.107.2.133   <none>       8528/TCP  0s
  arango-deployment-operator              ClusterIP  10.104.189.81  <none>       8528/TCP  0s

  ==> v1beta1/Deployment
  NAME                                    DESIRED  CURRENT  UP-TO-DATE  AVAILABLE  AGE
  arango-deployment-replication-operator  2        2        2           0          0s
  arango-deployment-operator              2        2        2           0          0s

  ==> v1/Pod(related)
  NAME                                                     READY  STATUS             RESTARTS  AGE
  arango-deployment-replication-operator-5f679fbfd8-nk8kz  0/1    Pending            0         0s
  arango-deployment-replication-operator-5f679fbfd8-pbxdl  0/1    ContainerCreating  0         0s
  arango-deployment-operator-65f969fc84-gjgl9              0/1    Pending            0         0s
  arango-deployment-operator-65f969fc84-wg4nf              0/1    ContainerCreating  0         0s


NOTES:

kube-arangodb has been deployed successfully!

Your release is named 'illocutionary-whippet'.

You can now deploy ArangoDeployment & ArangoDeploymentReplication resources.

See https://docs.arangodb.com/devel/Manual/Tutorials/Kubernetes/
for how to get started.
```
- As unlike cloud k8s offerings no file volume infrastructure exists, we need to still deploy the storage operator chart:

```
helm install \ 
	https://github.com/arangodb/kube-arangodb/releases/download/0.3.7/kube-arangodb-storage.tgz
```
```
  NAME:   sad-newt
  LAST DEPLOYED: Mon Jan 14 06:14:15 2019
  NAMESPACE: default
  STATUS: DEPLOYED

  RESOURCES:
  ==> v1/ServiceAccount
  NAME                     SECRETS  AGE
  arango-storage-operator  1        1s

  ==> v1beta1/CustomResourceDefinition
  NAME                                      AGE
  arangolocalstorages.storage.arangodb.com  1s

  ==> v1beta1/ClusterRole
  NAME                       AGE
  sad-newt-storages          1s
  sad-newt-storage-operator  1s

  ==> v1beta1/ClusterRoleBinding
  NAME                       AGE
  sad-newt-storage-operator  1s

  ==> v1beta1/RoleBinding
  NAME               AGE
  sad-newt-storages  1s

  ==> v1/Service
  NAME                     TYPE       CLUSTER-IP      EXTERNAL-IP  PORT(S)   AGE
  arango-storage-operator  ClusterIP  10.104.172.100  <none>       8528/TCP  1s

  ==> v1beta1/Deployment
  NAME                     DESIRED  CURRENT  UP-TO-DATE  AVAILABLE  AGE
  arango-storage-operator  2        2        2           0          1s

  ==> v1/Pod(related)
  NAME                                      READY  STATUS             RESTARTS  AGE
  arango-storage-operator-6bc64ccdfb-tzllq  0/1    ContainerCreating  0         0s
  arango-storage-operator-6bc64ccdfb-zdlxk  0/1    Pending            0         0s


  NOTES:

  kube-arangodb-storage has been deployed successfully!

  Your release is named 'sad-newt'.

  You can now deploy an ArangoLocalStorage resource.

  See https://docs.arangodb.com/devel/Manual/Deployment/Kubernetes/StorageResource.html
  for further instructions.

```
## Deploy ArangoDB cluster

- Deploy local storage

```
kubectl apply -f https://raw.githubusercontent.com/arangodb/kube-arangodb/master/examples/arango-local-storage.yaml
```
```
  arangolocalstorage.storage.arangodb.com/arangodb-local-storage created
```

- Deploy simple cluster

```
kubectl apply -f https://raw.githubusercontent.com/arangodb/kube-arangodb/master/examples/simple-cluster.yaml
```
```
  arangodeployment.database.arangodb.com/example-simple-cluster created
```

## Access your cluster

- Find your cluster's network address:

```
kubectl get services
```
```
  NAME                                     TYPE        CLUSTER-IP      EXTERNAL-IP   PORT(S)          AGE
  arango-deployment-operator               ClusterIP   10.104.189.81   <none>        8528/TCP         14m
  arango-deployment-replication-operator   ClusterIP   10.107.2.133    <none>        8528/TCP         14m
  example-simple-cluster                   ClusterIP   10.109.170.64   <none>        8529/TCP         5m18s
  example-simple-cluster-ea                NodePort    10.98.198.7     <none>        8529:30551/TCP   4m8s
  example-simple-cluster-int               ClusterIP   None            <none>        8529/TCP         5m19s
  kubernetes                               ClusterIP   10.96.0.1       <none>        443/TCP          69m
```

- In this case, according to the access service, `example-simple-cluster-ea`, the cluster's coordinators are reachable here:

https://kube01:30551, https://kube02:30551 and https://kube03:30551

## LoadBalancing

For this guide we like to use the `metallb` load balancer, which can be easiy installed as a simple layer 2 load balancer:

- install the `metalllb` controller: 

```
kubectl apply -f \
	https://raw.githubusercontent.com/google/metallb/v0.7.3/manifests/metallb.yaml
```
```
  namespace/metallb-system created
  serviceaccount/controller created
  serviceaccount/speaker created
  clusterrole.rbac.authorization.k8s.io/metallb-system:controller created
  clusterrole.rbac.authorization.k8s.io/metallb-system:speaker created
  role.rbac.authorization.k8s.io/config-watcher created
  clusterrolebinding.rbac.authorization.k8s.io/metallb-system:controller created
  clusterrolebinding.rbac.authorization.k8s.io/metallb-system:speaker created
  rolebinding.rbac.authorization.k8s.io/config-watcher created
  daemonset.apps/speaker created
  deployment.apps/controller created
```

- Deploy network range configurator. Assuming that the range for the IP addresses, which are granted to `metalllb` for load balancing is 192.168.10.224/28, download the [exmample layer2 configurator](https://raw.githubusercontent.com/google/metallb/v0.7.3/manifests/example-layer2-config.yaml).

```
wget https://raw.githubusercontent.com/google/metallb/v0.7.3/manifests/example-layer2-config.yaml
```

- Edit the `example-layer2-config.yaml` file to use the according addresses. Do this with great care, as YAML files are indention sensitive.  

```
apiVersion: v1
kind: ConfigMap
metadata:
  namespace: metallb-system
  name: config
data:
  config: |
    address-pools:
    - name: my-ip-space
      protocol: layer2
      addresses:
      - 192.168.10.224/28
```

- deploy the configuration map:

```
kubectl apply -f example-layer2-config.yaml
```
```
  configmap/config created
```

- restart ArangoDB's endpoint access service:

```
kubectl delete service example-simple-cluster-ea
```
```
  service "example-simple-cluster-ea" deleted
```

- watch, how the service goes from `Nodeport` to `LoadBalancer` the output above 

```
kubectl get services
```
```
  NAME                                     TYPE           CLUSTER-IP      EXTERNAL-IP      PORT(S)          AGE
  arango-deployment-operator               ClusterIP      10.104.189.81   <none>           8528/TCP         34m
  arango-deployment-replication-operator   ClusterIP      10.107.2.133    <none>           8528/TCP         34m
  example-simple-cluster                   ClusterIP      10.109.170.64   <none>           8529/TCP         24m
  example-simple-cluster-ea                LoadBalancer   10.97.217.222   192.168.10.224   8529:30292/TCP   22s
  example-simple-cluster-int               ClusterIP      None            <none>           8529/TCP         24m
  kubernetes                               ClusterIP      10.96.0.1       <none>           443/TCP          89m
```

- Now you are able of accessing all 3 coordinators through https://192.168.10.224:8529
