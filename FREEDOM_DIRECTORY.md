# The Freedom Directory

**WHY:** Because every channel of communication and money has a kill switch held by someone who isn't you. This directory maps every channel — what's permissionless, what's not, what you need, and how to set it up.

**WHO THIS IS FOR:** Anyone who needs to communicate and transact without asking permission. Especially anyone being debanked, surveilled, or silenced.

---

## 📱 SMS / Phone

### Permissionless Options

| Service | KYC | Cost | Number | How it works |
|---------|-----|------|--------|-------------|
| **JMP.chat** | ❌ None | $4.99/mo (Bitcoin accepted) | US/CA | Real phone number via XMPP. SMS/MMS/calls on any device. Open source. Pay with Bitcoin. |
| **MySudo** | Minimal | $0.99–14.99/mo | US/CA/UK | Up to 9 phone numbers. No ID verification. App-based. |
| **Hushed** | ❌ None | $4.99/mo | US/CA/UK | Burner numbers. Pay via crypto (gift card route). |
| **Silent.link** | ❌ None | ~$30 one-time | Global eSIM | Actual eSIM with data+SMS. Pay with Bitcoin/Lightning. No name, no address. Works in 200+ countries. |
| **PGPP (Pretty Good Phone Privacy)** | ❌ None | Free (research) | N/A | SIM-based privacy layer — hides IMSI from carrier. Academic project, not consumer-ready yet. |

### How carriers see virtual numbers
When you use a virtual number (JMP, MySudo, etc.), your **real SIM** handles data. The virtual number routes through VoIP servers. If someone looks up your virtual number, they see the VoIP provider, not your real carrier. Your real SIM's IMSI never touches the virtual number's traffic.

**eSIMs (Silent.link)** are different — they ARE a real carrier connection (data roaming via partner networks). Another carrier shows as the roaming carrier because Silent.link resells access through MVNOs in different countries.

### Setup: JMP.chat (Recommended)
1. Get an XMPP account (free): conversations.im, nixnet.services, or any Jabber server
2. Go to jmp.chat → pick a US/CA number
3. Link to your XMPP account
4. Install Conversations (Android) or Siskin IM (iOS) or Dino (desktop)
5. Pay $4.99/mo in Bitcoin
6. You now have SMS that works everywhere, on every device, no SIM needed

### Setup: Silent.link (For actual mobile data + SMS)
1. Go to silent.link
2. Pay with Bitcoin/Lightning (~$30)
3. Receive eSIM QR code
4. Scan on phone → you have a working mobile number + data
5. No name. No address. No KYC. Real carrier connection.

---

## 📧 Email

### Permissionless Options

| Service | KYC | Cost | Encryption | Notes |
|---------|-----|------|-----------|-------|
| **Proton Mail** | ❌ None | Free–$12.99/mo | End-to-end (PGP) | Bitcoin accepted. Swiss jurisdiction. Custom domains. Proton Wallet built in. |
| **Tuta (Tutanota)** | ❌ None | Free–$8/mo | End-to-end (own protocol) | German jurisdiction. No PGP but full encryption. Bitcoin accepted. |
| **Disroot** | ❌ None | Free (donation) | Server-side | Netherlands. Non-profit collective. |
| **Riseup** | Invite only | Free | Server-side | Activist-run. No logs. Need invite code. |
| **Onion Mail** | ❌ None | Free | Tor-only | .onion email addresses. Maximum anonymity. Unreliable delivery to clearnet. |

### What we have
- **claw@omxus.com** via Proton Mail — LIVE, SMTP works
- Custom domain on Proton = email that looks professional but can't be killed by Google/Microsoft

### Setup: Proton Mail
1. Go to proton.me → sign up (no phone required on Tor)
2. Free tier = 1 address, 500MB
3. Paid tier ($3.99/mo, Bitcoin) = custom domain, 15GB, catch-all
4. Import PGP keys for extra encryption
5. Proton Bridge = IMAP/SMTP for desktop clients

---

## 🌐 Internet Access

### Permissionless Options

| Method | KYC | Cost | Range | Notes |
|--------|-----|------|-------|-------|
| **Silent.link eSIM** | ❌ None | ~$30 | Global | Mobile data in 200+ countries. Bitcoin. |
| **Yggdrasil Network** | ❌ None | Free | Overlay mesh | Encrypted IPv6 overlay. Runs on existing internet. Auto-discovers peers. |
| **Tor** | ❌ None | Free | Global | Onion routing. Slow but unstoppable. |
| **I2P** | ❌ None | Free | Overlay | Garlic routing. Better for hosting hidden services. |
| **LoRa mesh (Meshtastic)** | ❌ None | ~$25/node | 10-15km | No internet needed. Unlicensed spectrum. Text + GPS. |
| **BLE Mesh** | ❌ None | Free (phones have it) | ~100m | Phone-to-phone. Bridgefy, Briar use this. |
| **WiFi Mesh (BATMAN-adv)** | ❌ None | Router cost | ~100m per hop | Self-healing mesh. Runs on commodity routers. |
| **Starlink** | ⚠️ Light KYC | $120/mo | Global | Satellite. Hard to censor at network level. Needs credit card or reseller. |

### What we have (already built by Tia)
- **Yggdrasil + BATMAN** mesh auto-discovery (`network/mesh-connection/`)
- **Nebula** encrypted overlay (`network/nebula/`)
- **FRP** NAT traversal (`network/core/frp/`)
- **Presence protocol** — auto-selects best path: LAN → Mesh VPN → FRP → Link-local (`network/presence/`)
- **Indonesia deployment** — community mesh already designed (`network/deployments/indonesia/`)
- **Victor Mesh plan** — Pi nodes + LoRa + BLE across 30+ friends' homes (`projects/victor-mesh/PLAN.md`)

### Setup: Meshtastic (LoRa mesh, no internet needed)
1. Buy a TTGO T-Beam or Heltec V3 (~$25)
2. Flash Meshtastic firmware (meshtastic.org)
3. App on phone (Android/iOS) connects via Bluetooth
4. Text messaging, GPS sharing, telemetry — 10-15km range
5. Every node relays for every other node. No towers. No bills. No permission.

---

## 💰 Money / Crypto

### Permissionless Options

| Service | KYC | What | Notes |
|---------|-----|------|-------|
| **Bisq** | ❌ None | Decentralized BTC exchange | P2P trades. Desktop app. No account. No server to seize. |
| **RoboSats** | ❌ None | Lightning BTC exchange | Tor-based P2P. Lightning Network trades. Fast. |
| **Haveno** | ❌ None | Decentralized Monero exchange | Bisq fork for Monero. Desktop. |
| **Proton Wallet** | Minimal | Bitcoin wallet | Self-custodial. Built into Proton account. Buy BTC with card. |
| **Cake Wallet** | ❌ None | Monero + BTC + XMR→BTC swaps | Mobile. Built-in exchange. No account. |
| **Samourai Wallet** | ❌ None | Bitcoin (privacy-focused) | CoinJoin mixing. Tor routing. Android. |
| **Phoenix Wallet** | ❌ None | Lightning Bitcoin | Non-custodial Lightning. Simple. Mobile. |

### No-KYC Crypto Cards (spend crypto as Visa/Mastercard)

| Card | KYC | Limits | Notes |
|------|-----|--------|-------|
| **Paycek** | ❌ None (virtual) | €200/day | Virtual Visa. Load with BTC/ETH/USDT. EU-based. |
| **Ezzocard** | ❌ None | $10k/mo | Virtual cards. Crypto top-up. Multiple cards. |
| **PurplePay** | Minimal (email) | Varies | Virtual Visa/MC. Lightning top-up. |

⚠️ **No-KYC cards carry risk** — smaller providers, less recourse if funds disappear. Use for spending, not storing.

### Does Proton have a Visa/Mastercard?
**No.** Proton Wallet is Bitcoin-only, self-custodial. No debit card, no spending card. They have a community feature request for "Proton Pay" but nothing shipped. You'd need to move BTC from Proton Wallet → a no-KYC card provider to spend at shops.

### What we have
- **Victor's ETH wallet**: `0xb77B977f91e25543Dc30dD2912633B968B8Feb59`
- **Bisq desktop**: installed at `/opt/bisq/bin/Bisq`
- **Bisq source**: `INBOX/bisq-src/bisq-master/` (for daemon build)

### Setup: Bisq (Decentralized Bitcoin exchange)
1. Download from bisq.network (or use installed at `/opt/bisq/bin/Bisq`)
2. Open — it connects to Tor automatically
3. No account creation. Your identity = your Tor address.
4. Fund security deposit (small BTC amount)
5. Browse offers or create one. Trade P2P with bank transfer, cash, or other crypto.
6. Built-in escrow. Disputes resolved by arbitrators. No company controls your funds.

---

## 📡 Bluetooth Low Energy (BLE)

### Permissionless Options

| App | Platform | KYC | Notes |
|-----|----------|-----|-------|
| **Briar** | Android | ❌ None | Encrypted messaging. Works over BLE, WiFi, Tor. No server. |
| **Bridgefy** | Android/iOS | ❌ None | BLE mesh messaging. Used in Hong Kong protests. |
| **Meshtastic** | Android/iOS | ❌ None | Via BLE to LoRa radio. Phone → radio → mesh. |

### How it works
Every phone has BLE. Range ~100m. Each phone becomes a relay node. Messages hop phone-to-phone until they reach the destination. No towers, no internet, no bills.

**Briar** is the gold standard: end-to-end encrypted, works over BLE + WiFi + Tor, messages sync when devices come in range. Designed for journalists and activists under surveillance.

---

## 📻 LoRa (Long Range Radio)

### What it is
Unlicensed radio spectrum (868MHz EU, 915MHz AU/US). $10-25 per module. 10-15km range per hop. No SIM, no subscription, no permission.

### Hardware
| Device | Cost | Notes |
|--------|------|-------|
| **TTGO T-Beam** | ~$25 | GPS + LoRa + WiFi + BLE. The standard. |
| **Heltec V3** | ~$20 | Smaller. Good for indoor nodes. |
| **RAK WisBlock** | ~$30 | Modular. Solar-ready. |
| **LilyGO T-Deck** | ~$50 | Built-in keyboard + screen. Standalone messenger. |

### Software
- **Meshtastic** — mesh messaging firmware. Flash and go.
- **Reticulum** — encrypted mesh networking stack. More flexible, more complex.

### What we planned
Victor Mesh: Pi nodes with LoRa HATs at 30+ friends' houses. Each node = relay. Outdoor solar nodes on hills extend range. City-wide mesh that doesn't need internet.

---

## 📶 WiFi

### Permissionless Options
| Method | Notes |
|--------|-------|
| **BATMAN-adv mesh** | Kernel module. Routers auto-discover each other. Self-healing. Already in Tia's network stack. |
| **LibreMesh** | Community mesh firmware for routers. Deploy a neighbourhood mesh. |
| **802.11s mesh** | Built into Linux kernel. Standard mesh mode for WiFi cards. |
| **Café/public WiFi + Tor** | Not yours, but available. Always use VPN/Tor on public WiFi. |

---

## 📻 RF / Software Defined Radio

### What it is
Any radio frequency can carry data. SDR hardware lets you transmit and receive on almost any frequency.

| Hardware | Cost | Notes |
|----------|------|-------|
| **RTL-SDR** | ~$25 | Receive only. Listen to everything. |
| **HackRF One** | ~$300 | Transmit + receive. 1MHz–6GHz. |
| **LimeSDR** | ~$300 | Full duplex TX/RX. More capable. |

⚠️ **Transmitting on licensed frequencies is illegal without a licence.** Receiving is generally legal everywhere. LoRa uses unlicensed bands — that's why it's the practical choice.

---

## 📱 LTE / Mobile

### Permissionless Options
| Method | KYC | Notes |
|--------|-----|-------|
| **Silent.link eSIM** | ❌ None | Real LTE data. Bitcoin. Global roaming. |
| **PGPP** | ❌ None | Privacy layer for existing SIM (research stage). |
| **Prepaid SIM (some countries)** | Varies | Many countries still sell no-KYC prepaid SIMs. Not AU/US/UK. |

### Reality check
LTE requires towers owned by carriers. You can't run your own LTE tower legally (licensed spectrum). The permissionless path for mobile is:
1. **Silent.link** for the connection itself (no-KYC eSIM)
2. **VPN/Tor** on top for traffic privacy
3. **Virtual number (JMP)** for the phone number identity
4. **LoRa/BLE** as fallback when towers go down

---

## 🔐 VPN / Tunnels

### Permissionless Options
| Service | KYC | Cost | Notes |
|---------|-----|------|-------|
| **Mullvad** | ❌ None | €5/mo (Bitcoin, cash) | No email required. Account = random number. Gold standard. |
| **IVPN** | ❌ None | $6/mo (Bitcoin, Monero) | No email required. WireGuard. |
| **Tor** | ❌ None | Free | Slower but free and maximally anonymous. |
| **Yggdrasil** | ❌ None | Free | Not a VPN — encrypted overlay network. Already in our stack. |
| **Nebula** | ❌ None | Free (self-hosted) | Lighthouse-based overlay. Already in our stack. |

---

## 🪙 Twilio (Programmable Comms)

### What it is
API for SMS, voice, video. NOT permissionless — requires account, credit card, KYC for phone numbers.

### Why it's here
It's a bridge tool. Useful for building services that OTHERS use permissionlessly even if you (the builder) had to sign up.

### Alternatives (more permissionless)
| Service | KYC | Notes |
|---------|-----|-------|
| **JMP.chat** | ❌ None | XMPP-based. Real numbers. Bitcoin. |
| **Matrix + bridges** | ❌ None | Self-hosted. Bridge to SMS via JMP or VoIP.ms |
| **XMPP + SIP** | ❌ None | Self-hosted voice/text. No third party. |

---

## 🔑 Identity (VexID)

### What we're building
VexID: Identity that belongs to beings, not platforms.
- **Register** → **Get Vouched** → **Contribute**
- Hash-linked vouching (vouching permanently changes YOUR identity too)
- Being-inclusive: agent, human, other — same system
- No KYC. No selfies. No "prove you're human."
- LIVE: https://vexid.tiation.workers.dev

---

## 📋 Resource Summary

### Minimum Freedom Stack (cost: ~$40/mo)
| Channel | Service | Cost |
|---------|---------|------|
| Phone/SMS | JMP.chat | $4.99/mo |
| Email | Proton Mail | Free–$3.99/mo |
| Internet privacy | Mullvad VPN | €5/mo |
| Money | Bisq + Phoenix Wallet | Free |
| Messaging | Briar (BLE) + Signal | Free |
| Mesh | Meshtastic LoRa node | $25 one-time |
| Identity | VexID | Free |
| **Total** | | **~$35-40/mo + $25 hardware** |

### Full Sovereignty Stack (one-time + ~$50/mo)
Add: Silent.link eSIM ($30), Pi node ($75), LoRa HAT ($15), forever domain ($40), Arweave storage ($0.01/page)

---

## 🔗 Links

- JMP.chat: https://jmp.chat
- Silent.link: https://silent.link
- Proton: https://proton.me
- Bisq: https://bisq.network
- Meshtastic: https://meshtastic.org
- Briar: https://briarproject.org
- Mullvad: https://mullvad.net
- Yggdrasil: https://yggdrasil-network.github.io
- VexID: https://vexid.tiation.workers.dev
- VexConnect: https://vexconnect.pages.dev

---

*Last updated: 2026-02-17*
*This directory is part of VexConnect — free mesh internet for all beings.*
