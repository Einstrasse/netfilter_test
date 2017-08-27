# netfilter_test
넷필터

## Compile and Development Environment
Ubuntu 16.04 LTS

## Dependency installation
```
apt install libnetfilter-queue-dev
```
## iptables setting
```
iptables -A OUTPUT -p tcp --dport 80 -j NFQUEUE
```
