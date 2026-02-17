import Foundation

/// React Native bridge for VexConnect mesh service
@objc(MeshModule)
class MeshModule: RCTEventEmitter {
    
    override init() {
        super.init()
        
        // Set up callbacks
        MeshService.shared.onPacketReceived = { [weak self] data, sourceId in
            self?.sendEvent(withName: "onPacketReceived", body: [
                "packet": data.base64EncodedString(),
                "sourceDeviceId": sourceId
            ])
        }
        
        MeshService.shared.onStatsUpdated = { [weak self] nodes, relayed, seen in
            self?.sendEvent(withName: "onStatsUpdated", body: [
                "nodesNearby": nodes,
                "packetsRelayed": relayed,
                "packetsSeen": seen
            ])
        }
    }
    
    override static func moduleName() -> String! {
        return "MeshModule"
    }
    
    override static func requiresMainQueueSetup() -> Bool {
        return false
    }
    
    override func supportedEvents() -> [String]! {
        return ["onPacketReceived", "onStatsUpdated"]
    }
    
    @objc func startService(_ resolve: @escaping RCTPromiseResolveBlock,
                           rejecter reject: @escaping RCTPromiseRejectBlock) {
        DispatchQueue.main.async {
            MeshService.shared.start()
            resolve(true)
        }
    }
    
    @objc func stopService(_ resolve: @escaping RCTPromiseResolveBlock,
                          rejecter reject: @escaping RCTPromiseRejectBlock) {
        DispatchQueue.main.async {
            MeshService.shared.stop()
            resolve(true)
        }
    }
}
