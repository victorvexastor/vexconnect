# The Stack — Everything a $25 Computer Can Do

## Communication Layers

| Layer | Tech | Range | Speed | Cost | ID needed |
|-------|------|-------|-------|------|-----------|
| 1 | **Bluetooth** | 10–100m | Fast (2 Mbps) | $0 (built in) | No |
| 2 | **WiFi** | 30–50m | Very fast | $0 (built in) | No |
| 3 | **LoRa** | 2–15km | Slow (300 B/s) | $10 module | No |
| 4 | **Yggdrasil** | Global (mesh over internet) | Fast | $0 (software) | No |
| 5 | **4G** | Cell coverage | Fast | $10/month | SIM |

**Five layers. Four cost nothing. Every node has at least 3 available at all times.**

### The cheapest to most expensive:

```
Same room:    Bluetooth    (free, built in)
Same street:  WiFi         (free, built in)
Same city:    LoRa         ($10, forever)
Anywhere:     Yggdrasil    (free, mesh over internet)
Backup:       4G           (one SIM, $10/month)
```

### What Bluetooth does:

Bluetooth is already in every Pi Zero 2 W. Free. No extra hardware.

- **Two nodes in the same house or next-door neighbours** — Bluetooth mesh between them, save WiFi for internet
- **A Pi talking to your phone directly** — no WiFi, no internet needed
- **Bluetooth tethering** — your phone shares its data connection to a Pi via Bluetooth. Instant 4G for the node, no SIM in the Pi.

Bluetooth fills the gap between "same room" and LoRa. It's the glue.

---

## Hardware

### Node: Raspberry Pi Zero 2 W — $25

| Spec | Value |
|------|-------|
| CPU | ARM Cortex-A53 quad-core 1GHz |
| RAM | 512MB |
| WiFi | 802.11 b/g/n |
| Bluetooth | BLE 4.2 |
| Power | 5V USB, ~0.5W idle |
| Size | 65mm × 30mm (credit card) |
| Storage | MicroSD |
| GPIO | 40 pins (SPI for LoRa) |

Runs headless Linux. Fits in your palm. Solar powered with a $15 panel.

### Micro Node: Raspberry Pi Pico W — $6

| Spec | Value |
|------|-------|
| CPU | RP2040 dual-core ARM Cortex-M0+ 133MHz |
| RAM | 264KB |
| WiFi | 802.11 b/g/n |
| Bluetooth | BLE 5.2 |
| Power | ~0.02W deep sleep |
| Size | 51mm × 21mm (smaller than your thumb) |
| Storage | 2MB flash |
| GPIO | 26 pins |

$6. BLE + WiFi. Runs on a coin cell for weeks. Perfect for:
- **Doorbell mesh nodes** — stick one on a fence post, relays BLE packets
- **Sensor nodes** — temperature, motion, light + mesh relay in one
- **Disposable relays** — at $6, you can scatter them

No Linux. Runs MicroPython or C directly. Boots in milliseconds.

### LoRa Module: SX1276/SX1262 — $10

| Spec | Value |
|------|-------|
| Range | 2–15km (line of sight) |
| Frequency | 915MHz AU (unlicensed) |
| Speed | 300 bytes/sec |
| Power | ~0.05W transmit |
| Interface | SPI (connects to Pi GPIO) |

Whispers across kilometres. No SIM. No licence. Just radio waves on public spectrum.

### Solar Kit — $15–25

| Component | Cost |
|-----------|------|
| 6V 1W solar panel | $8 |
| TP4056 charge controller | $2 |
| 18650 lithium cell | $5 |
| Weatherproof case | $5–10 |

Runs a Pi Zero 2 W indefinitely in Australian sun. No power bill. No mains. Plant it on a pole and forget about it.

---

## Software Stack

### On the Node

| Software | What it does | Cost |
|----------|-------------|------|
| **VexConnect** | BLE + LoRa mesh relay (our C binary, 58KB) | Free, open source |
| **Yggdrasil** | Encrypted mesh overlay network (IPv6) | Free, open source |
| **Tor** | Anonymous routing when internet available | Free, open source |
| **WireGuard** | Encrypted tunnels between nodes | Free, open source |

### On Your Phone

| App | What it does | Cost |
|-----|-------------|------|
| **VexConnect app** | BLE mesh relay — your phone becomes a node | Free |
| **Briar** | Encrypted messaging over Bluetooth/WiFi/Tor | Free |
| **Meshtastic** | LoRa mesh messaging (if you have a radio) | Free |

### For Identity

| Service | What it does | Cost |
|---------|-------------|------|
| **VexID** | Sovereign identity — vouched by community, not government | Free |
| **Proton Mail** | Encrypted email — Bitcoin accepted, no KYC | Free–$12.99/mo |
| **Silent.link** | Anonymous eSIM — Bitcoin, no name, 200+ countries | ~$30 one-time |
| **JMP.chat** | Phone number via XMPP — Bitcoin, no KYC | $4.99/mo |

### For Domains & Hosting

| Service | What it does | Cost |
|---------|-------------|------|
| **Njal.la** | Anonymous domain registration — Bitcoin, no personal details, Icelandic privacy law | $15/yr |
| **Unstoppable Domains** | Forever domains (.x, .crypto) — one-time purchase, no renewal, on blockchain | $5–40 one-time |
| **Handshake** | Decentralised DNS — own a TLD, no ICANN | Auction |
| **Cloudflare Pages** | Free static hosting — no credit card needed | Free |
| **IPFS** | Decentralised file storage — content-addressed, uncensorable | Free |
| **Arweave** | Permanent storage — pay once, stored forever | ~$0.01/page |

### For Money

| Service | What it does | Cost |
|---------|-------------|------|
| **Bitcoin** | Permissionless money — no bank, no KYC (if bought P2P) | Free |
| **Bisq** | Decentralised Bitcoin exchange — no KYC, P2P | Free |
| **Monero** | Private cryptocurrency — untraceable transactions | Free |
| **Lightning Network** | Instant Bitcoin payments — $0.01 fees | Free |

---

## What It Costs — Per Community

### Minimum Viable Mesh (2 locations)

| Item | Qty | Unit | Total |
|------|-----|------|-------|
| Pi Zero 2 W | 4 | $25 | $100 |
| LoRa SX1276 | 4 | $10 | $40 |
| Solar kit | 4 | $20 | $80 |
| SD cards 32GB | 4 | $8 | $32 |
| Pico W relay nodes | 10 | $6 | $60 |
| Weatherproof cases | 4 | $8 | $32 |
| **Total** | | | **$344** |

That's two locations connected, 10 micro-relays filling gaps, running forever on solar.

### Full Sovereign Stack (per person)

| Item | Cost | Frequency |
|------|------|-----------|
| Phone (already have) | $0 | — |
| VexConnect app | $0 | — |
| Proton Mail | $0 | — |
| VexID | $0 | — |
| Silent.link eSIM | $30 | One-time |
| Njal.la domain | $15 | Per year |
| Unstoppable Domain | $20 | One-time (forever) |
| **Year 1** | **$65** | |
| **Year 2+** | **$15** | Just domain renewal (or $0 with Unstoppable) |

### Full Network (2 communities, 50 people)

| Item | Cost |
|------|------|
| Mesh hardware (above) | $344 |
| Local AI server (128GB RAM, GPU) | $1,500 |
| Forever domains | $50 |
| Dev time (C binary, 80hrs) | $1,200 |
| **Total** | **$3,094** |

Monthly running cost: **$5–10 electricity.** That's it. Forever.

---

## The Point

Every phone is a radio. Every radio is a node. Every node extends the mesh.

No tower. No ISP. No carrier. No bill. No permission. No log. No ID.

Just radio waves and cooperation.

A community of 50 people with 4 Pi nodes and 10 Pico relays has:
- Their own encrypted communications network
- Their own identity system
- Their own AI (local, no cloud)
- Their own domain names (forever, uncensorable)
- Total cost less than what 10 people pay Telstra in a year

**The hardware exists. The software exists. The spectrum is public. The only thing missing is the decision to build it.**
