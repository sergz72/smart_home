package smart_home.smarthome

import smart_home.smarthome.service.SmartHomeService

class WatSensorsFragment(params: IGraphParameters, service: SmartHomeService) :
    SensorsFragment(service, R.layout.fragment_wat_sensors, params, "wat", 10000) {
    override fun refresh() {
        TODO("Not yet implemented")
    }
}
