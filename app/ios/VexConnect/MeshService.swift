import Foundation
import CoreBluetooth

/// VexConnect Mesh Service for iOS
/// Handles BLE peripheral (GATT server) and central operations
@objc(MeshService)
class MeshService: NSObject {
    
    // MARK: - Constants
    
    static let serviceUUID = CBUUID(string: "0000VC01-0000-1000-8000-00805F9B34FB")
    static let txCharUUID = CBUUID(string: "0000VC02-0000-1000-8000-00805F9B34FB")
    static let rxCharUUID = CBUUID(string: "0000VC03-0000-1000-8000-00805F9B34FB")
    
    static let protocolVersion: UInt8 = 0x01
    static let defaultTTL: UInt8 = 7
    static let seenCacheMax = 1000
    static let seenCacheTTL: TimeInterval = 60.0
    
    // MARK: - Singleton
    
    @objc static let shared = MeshService()
    
    // MARK: - Properties
    
    private var peripheralManager: CBPeripheralManager?
    private var centralManager: CBCentralManager?
    
    private var txCharacteristic: CBMutableCharacteristic?
    private var rxCharacteristic: CBMutableCharacteristic?
    
    private var connectedCentrals: [CBCentral] = []
    private var connectedPeripherals: [CBPeripheral] = []
    
    private var seenCache: [String: Date] = [:]
    private var isRunning = false
    
    // Stats
    private(set) var packetsRelayed = 0
    private(set) var packetsSeen = 0
    
    // Callbacks
    var onPacketReceived: ((Data, String) -> Void)?
    var onStatsUpdated: ((Int, Int, Int) -> Void)?
    
    // MARK: - Lifecycle
    
    private override init() {
        super.init()
    }
    
    @objc func start() {
        guard !isRunning else { return }
        isRunning = true
        
        // Initialize managers
        peripheralManager = CBPeripheralManager(delegate: self, queue: nil, options: [
            CBPeripheralManagerOptionRestoreIdentifierKey: "com.vexconnect.peripheral"
        ])
        
        centralManager = CBCentralManager(delegate: self, queue: nil, options: [
            CBCentralManagerOptionRestoreIdentifierKey: "com.vexconnect.central"
        ])
        
        // Start cache cleanup timer
        Timer.scheduledTimer(withTimeInterval: 30.0, repeats: true) { [weak self] _ in
            self?.cleanSeenCache()
        }
        
        print("[VexConnect] Mesh started")
    }
    
    @objc func stop() {
        isRunning = false
        
        peripheralManager?.stopAdvertising()
        peripheralManager = nil
        
        centralManager?.stopScan()
        centralManager = nil
        
        connectedCentrals.removeAll()
        connectedPeripherals.removeAll()
        seenCache.removeAll()
        
        print("[VexConnect] Mesh stopped")
    }
    
    // MARK: - GATT Server Setup
    
    private func setupGATTServer() {
        guard let pm = peripheralManager else { return }
        
        // TX characteristic - other nodes write to us
        txCharacteristic = CBMutableCharacteristic(
            type: MeshService.txCharUUID,
            properties: [.write, .writeWithoutResponse],
            value: nil,
            permissions: [.writeable]
        )
        
        // RX characteristic - we notify others
        rxCharacteristic = CBMutableCharacteristic(
            type: MeshService.rxCharUUID,
            properties: [.read, .notify],
            value: nil,
            permissions: [.readable]
        )
        
        let service = CBMutableService(type: MeshService.serviceUUID, primary: true)
        service.characteristics = [txCharacteristic!, rxCharacteristic!]
        
        pm.add(service)
    }
    
    private func startAdvertising() {
        peripheralManager?.startAdvertising([
            CBAdvertisementDataServiceUUIDsKey: [MeshService.serviceUUID],
            CBAdvertisementDataLocalNameKey: "VexConnect"
        ])
        print("[VexConnect] Advertising started")
    }
    
    // MARK: - Packet Handling
    
    private func handleIncomingPacket(_ data: Data, from source: String) {
        guard data.count >= 11 else { return }
        
        // Parse header
        let version = data[0]
        guard version == MeshService.protocolVersion else { return }
        
        // Extract packet ID
        let packetIdData = data.subdata(in: 1..<9)
        let packetId = packetIdData.map { String(format: "%02x", $0) }.joined()
        
        // Deduplication
        if seenCache[packetId] != nil { return }
        seenCache[packetId] = Date()
        packetsSeen += 1
        
        // TTL check
        let ttl = data[9]
        guard ttl > 1 else { return }
        
        // Decrement TTL and relay
        var relayData = data
        relayData[9] = ttl - 1
        
        relayPacket(relayData, excluding: source)
        
        // Notify callback
        onPacketReceived?(data, source)
        notifyStats()
    }
    
    private func relayPacket(_ packet: Data, excluding sourceId: String) {
        guard let rxChar = rxCharacteristic else { return }
        
        rxChar.value = packet
        
        // Notify all subscribed centrals
        for central in connectedCentrals {
            let success = peripheralManager?.updateValue(
                packet,
                for: rxChar,
                onSubscribedCentrals: [central]
            ) ?? false
            
            if success {
                packetsRelayed += 1
            }
        }
    }
    
    private func cleanSeenCache() {
        let cutoff = Date().addingTimeInterval(-MeshService.seenCacheTTL)
        seenCache = seenCache.filter { $0.value > cutoff }
        
        // LRU eviction
        while seenCache.count > MeshService.seenCacheMax {
            if let oldest = seenCache.min(by: { $0.value < $1.value })?.key {
                seenCache.removeValue(forKey: oldest)
            }
        }
    }
    
    private func notifyStats() {
        let nodes = connectedCentrals.count + connectedPeripherals.count
        onStatsUpdated?(nodes, packetsRelayed, packetsSeen)
    }
}

// MARK: - CBPeripheralManagerDelegate

extension MeshService: CBPeripheralManagerDelegate {
    
    func peripheralManagerDidUpdateState(_ peripheral: CBPeripheralManager) {
        switch peripheral.state {
        case .poweredOn:
            print("[VexConnect] Peripheral powered on")
            setupGATTServer()
        case .poweredOff:
            print("[VexConnect] Peripheral powered off")
        case .unauthorized:
            print("[VexConnect] Bluetooth unauthorized")
        default:
            break
        }
    }
    
    func peripheralManager(_ peripheral: CBPeripheralManager, didAdd service: CBService, error: Error?) {
        if let error = error {
            print("[VexConnect] Failed to add service: \(error)")
            return
        }
        print("[VexConnect] Service added, starting advertising")
        startAdvertising()
    }
    
    func peripheralManagerDidStartAdvertising(_ peripheral: CBPeripheralManager, error: Error?) {
        if let error = error {
            print("[VexConnect] Advertising failed: \(error)")
        }
    }
    
    func peripheralManager(_ peripheral: CBPeripheralManager, central: CBCentral, didSubscribeTo characteristic: CBCharacteristic) {
        print("[VexConnect] Central subscribed: \(central.identifier)")
        connectedCentrals.append(central)
        notifyStats()
    }
    
    func peripheralManager(_ peripheral: CBPeripheralManager, central: CBCentral, didUnsubscribeFrom characteristic: CBCharacteristic) {
        print("[VexConnect] Central unsubscribed: \(central.identifier)")
        connectedCentrals.removeAll { $0.identifier == central.identifier }
        notifyStats()
    }
    
    func peripheralManager(_ peripheral: CBPeripheralManager, didReceiveWrite requests: [CBATTRequest]) {
        for request in requests {
            if request.characteristic.uuid == MeshService.txCharUUID,
               let value = request.value {
                handleIncomingPacket(value, from: request.central.identifier.uuidString)
            }
            peripheral.respond(to: request, withResult: .success)
        }
    }
    
    // State restoration
    func peripheralManager(_ peripheral: CBPeripheralManager, willRestoreState dict: [String: Any]) {
        print("[VexConnect] Peripheral state restored")
        if let services = dict[CBPeripheralManagerRestoredStateServicesKey] as? [CBMutableService] {
            for service in services {
                for char in service.characteristics ?? [] {
                    if let mutableChar = char as? CBMutableCharacteristic {
                        if char.uuid == MeshService.txCharUUID {
                            txCharacteristic = mutableChar
                        } else if char.uuid == MeshService.rxCharUUID {
                            rxCharacteristic = mutableChar
                        }
                    }
                }
            }
        }
    }
}

// MARK: - CBCentralManagerDelegate

extension MeshService: CBCentralManagerDelegate {
    
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        switch central.state {
        case .poweredOn:
            print("[VexConnect] Central powered on, starting scan")
            central.scanForPeripherals(
                withServices: [MeshService.serviceUUID],
                options: [CBCentralManagerScanOptionAllowDuplicatesKey: false]
            )
        default:
            break
        }
    }
    
    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral, advertisementData: [String: Any], rssi RSSI: NSNumber) {
        print("[VexConnect] Discovered: \(peripheral.identifier) RSSI: \(RSSI)")
        
        if !connectedPeripherals.contains(where: { $0.identifier == peripheral.identifier }) {
            connectedPeripherals.append(peripheral)
            peripheral.delegate = self
            central.connect(peripheral, options: nil)
        }
    }
    
    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        print("[VexConnect] Connected to: \(peripheral.identifier)")
        peripheral.discoverServices([MeshService.serviceUUID])
        notifyStats()
    }
    
    func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        print("[VexConnect] Disconnected from: \(peripheral.identifier)")
        connectedPeripherals.removeAll { $0.identifier == peripheral.identifier }
        notifyStats()
        
        // Reconnect if still running
        if isRunning {
            central.connect(peripheral, options: nil)
        }
    }
    
    // State restoration
    func centralManager(_ central: CBCentralManager, willRestoreState dict: [String: Any]) {
        print("[VexConnect] Central state restored")
        if let peripherals = dict[CBCentralManagerRestoredStatePeripheralsKey] as? [CBPeripheral] {
            connectedPeripherals = peripherals
            for peripheral in peripherals {
                peripheral.delegate = self
            }
        }
    }
}

// MARK: - CBPeripheralDelegate

extension MeshService: CBPeripheralDelegate {
    
    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        guard let services = peripheral.services else { return }
        
        for service in services where service.uuid == MeshService.serviceUUID {
            peripheral.discoverCharacteristics(
                [MeshService.txCharUUID, MeshService.rxCharUUID],
                for: service
            )
        }
    }
    
    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        guard let characteristics = service.characteristics else { return }
        
        for char in characteristics {
            if char.uuid == MeshService.rxCharUUID {
                peripheral.setNotifyValue(true, for: char)
            }
        }
    }
    
    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        if characteristic.uuid == MeshService.rxCharUUID,
           let value = characteristic.value {
            handleIncomingPacket(value, from: peripheral.identifier.uuidString)
        }
    }
}
