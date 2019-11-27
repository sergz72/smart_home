package smart_home.smarthome.entities

import com.google.gson.JsonDeserializationContext
import com.google.gson.JsonDeserializer
import com.google.gson.JsonElement
import com.google.gson.JsonParseException

import java.lang.reflect.Type
import java.text.ParseException
import java.text.SimpleDateFormat
import java.util.*

class DateDeserializer : JsonDeserializer<Date> {

    @Throws(JsonParseException::class)
    override fun deserialize(jsonElement: JsonElement?, typeOF: Type, context: JsonDeserializationContext): Date? {
        try {
            if (jsonElement == null || jsonElement.isJsonNull) {
                return null
            }
            val s = jsonElement.asString
            return FORMATTER.parse(s)
        } catch (e: ParseException) {
            throw JsonParseException("Unparseable date: \"" + jsonElement!!.asString
                    + "\". Supported formats: " + DATETIMEFORMAT)
        }

    }

    companion object {
        private const val DATETIMEFORMAT = "yyyy-MM-dd HH:mm"
        private val FORMATTER = SimpleDateFormat(DATETIMEFORMAT, Locale.US)
    }
}

