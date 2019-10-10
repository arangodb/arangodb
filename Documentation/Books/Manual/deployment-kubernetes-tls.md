---
layout: default
description: The ArangoDB Kubernetes Operator will by default create ArangoDB deploymentsthat use secure TLS connections
---

# Secure connections (TLS)

The ArangoDB Kubernetes Operator will by default create ArangoDB deployments
that use secure TLS connections.

It uses a single CA certificate (stored in a Kubernetes secret) and
one certificate per ArangoDB server (stored in a Kubernetes secret per server).

To disable TLS, set `spec.tls.caSecretName` to `None`.

## Install CA certificate

If the CA certificate is self-signed, it will not be trusted by browsers,
until you install it in the local operating system or browser.
This process differs per operating system.

To do so, you first have to fetch the CA certificate from its Kubernetes
secret.

{% raw %}
```bash
kubectl get secret <deploy-name>-ca --template='{{index .data "ca.crt"}}' | base64 -D > ca.crt
```
{% endraw %}

### Windows

To install a CA certificate in Windows, follow the
[procedure described here](http://wiki.cacert.org/HowTo/InstallCAcertRoots){:target="_blank"}.

### macOS

To install a CA certificate in macOS, run:

```bash
sudo /usr/bin/security add-trusted-cert -d -r trustRoot -k /Library/Keychains/System.keychain ca.crt
```

To uninstall a CA certificate in macOS, run:

```bash
sudo /usr/bin/security remove-trusted-cert -d ca.crt
```

### Linux

To install a CA certificate in Linux, on Ubuntu, run:

```bash
sudo cp ca.crt /usr/local/share/ca-certificates/<some-name>.crt
sudo update-ca-certificates
```

## See also

- [Authentication](deployment-kubernetes-authentication.html)
