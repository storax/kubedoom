# Kube DOOM
## Kill Kubernetes pods using Id's Doom!

The next level of chaos engineering is here! Kill pods inside your Kubernetes
cluster by shooting them in Doom!

This is a fork of the excellent
[gideonred/dockerdoomd](https://github.com/gideonred/dockerdoomd) using a
slightly modified Doom, forked from https://github.com/gideonred/dockerdoom,
which was forked from psdoom.

## Usage

Run the `storaxdev/kubedoom:0.1.0` docker image inside your cluster and expose
port 5900. Then connect with a VNC viewer to it.

You can quickly test it using [kind](https://github.com/kubernetes-sigs/kind).
Create a cluster with the example config from this repository:

```console
$ kind create cluster --config kind-config.yaml
Creating cluster "kind" ...
 ✓ Ensuring node image (kindest/node:v1.15.0) 🖼
 ✓ Preparing nodes 📦📦
 ✓ Creating kubeadm config 📜
 ✓ Starting control-plane 🕹️
 ✓ Installing CNI 🔌
 ✓ Installing StorageClass 💾
 ✓ Joining worker nodes 🚜
Cluster creation complete. You can now use the cluster with:

export KUBECONFIG="$(kind get kubeconfig-path --name="kind")"
kubectl cluster-info
```

This will spin up a 2 node cluster inside docker, with port 5900 exposed from
the worker node. Then run kubedoom inside the cluster by applying the manifest
provided in this repository:

```console
$ export KUBECONFIG="$(kind get kubeconfig-path --name="kind")"
$ kubectl apply -f manifest/
namespace/kubedoom created
deployment.apps/kubedoom created
serviceaccount/kubedoom created
clusterrolebinding.rbac.authorization.k8s.io/kubedoom created
```

Now start a VNC viewer and connect to `localhost:5900`. The password is `1234`:
```console
$ vncviewer viewer localhost
```

You should now see DOOM! Now if you want to get the job done quickly enter the
cheat `idspispopd` and walk through the wall on your right. You should be
greeted by your pods as little pink monsters. Press `CTRL` to fire. If the
pistol is not your thing, cheat with `idkfa` and press `5` for a nice surprise.
Pause the game with `ESC`.

Kubedoom requires a service account with permissions to list all pods and delete
them and uses kubectl 1.15.3.
