package smart_home.smarthome

import android.app.Activity
import android.app.AlertDialog
import android.content.Intent
import android.os.Bundle
import android.view.Menu
import android.view.MenuItem
import android.view.View
import android.widget.AdapterView
import android.widget.ArrayAdapter
import android.widget.Spinner
import androidx.activity.OnBackPressedCallback
import androidx.activity.result.ActivityResult
import androidx.activity.result.ActivityResultCallback
import androidx.activity.result.ActivityResultLauncher
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.ActionBarDrawerToggle
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.widget.Toolbar
import androidx.core.view.GravityCompat
import androidx.drawerlayout.widget.DrawerLayout
import com.google.android.material.navigation.NavigationView

class MainActivity : AppCompatActivity(), NavigationView.OnNavigationItemSelectedListener, IGraphParameters,
    ActivityResultCallback<ActivityResult>, AdapterView.OnItemSelectedListener {
    companion object {
        const val PREFS_NAME = "SmartHome"
        private const val SETTINGS = 1
        private const val FILTERS  = 2
        private const val PAGE_MAX = 3

        fun alert(activity: Activity, message: String?) {
            val builder = AlertDialog.Builder(activity)
            builder.setMessage(message)
                    .setTitle("Error")
            val dialog = builder.create()
            dialog.show()
        }
    }

    private var pageId: Int = 0
    private var filters: FiltersActivity.Data = FiltersActivity.Data()
    private var mActivityResultLauncher: ActivityResultLauncher<Intent>? = null
    private var mServerList: Spinner? = null
    private var disableRefresh = true

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        val toolbar = findViewById<Toolbar>(R.id.toolbar)
        setSupportActionBar(toolbar)

        val drawer = findViewById<DrawerLayout>(R.id.drawer_layout)
        val toggle = ActionBarDrawerToggle(
                this, drawer, toolbar, R.string.navigation_drawer_open, R.string.navigation_drawer_close)
        drawer.addDrawerListener(toggle)
        toggle.syncState()

        val navigationView = findViewById<NavigationView>(R.id.nav_view)
        navigationView.setNavigationItemSelectedListener(this)

        SmartHomeService.setupKey(resources.openRawResource(R.raw.key))
        SmartHomeServiceV2.setupKey(resources.openRawResource(R.raw.keyv2))

        mServerList = findViewById(R.id.serverSelection)
        mServerList!!.onItemSelectedListener = this

        mActivityResultLauncher = registerForActivityResult(ActivityResultContracts.StartActivityForResult(), this)

        onBackPressedDispatcher.addCallback(this, object : OnBackPressedCallback(true) {
            override fun handleOnBackPressed() {
                val drawerr = findViewById<DrawerLayout>(R.id.drawer_layout)
                if (drawerr.isDrawerOpen(GravityCompat.START)) {
                    drawerr.closeDrawer(GravityCompat.START)
                }
            }
        })

        try {
            updateServer(false)
            openPage(0)
        } catch (e: Exception) {
            alert(this, e.message)
        }
    }

    private fun updateServer(refreshData: Boolean) {
        val settings = getSharedPreferences(PREFS_NAME, 0)

        val serverList = IntRange(1, 3)
            .filter { settings.getString("server$it", "")!!.isNotEmpty() }
            .map { settings.getString("server_name$it", "Server $it") }
        val adapter = ArrayAdapter(this, R.layout.spinner_item, serverList)
        mServerList!!.adapter = adapter

        val defaultServer = settings.getString("default_server", "1")!!.toIntOrNull()
        if (defaultServer != null && defaultServer <= serverList.size) {
            mServerList!!.setSelection(defaultServer - 1)
        }

        refresh(refreshData)
    }

    private fun refresh(refreshData: Boolean) {
        val serverNameObject = mServerList!!.selectedItem ?: return
        val settings = getSharedPreferences(PREFS_NAME, 0)
        val serverName = serverNameObject as String

        var serverAddress = IntRange(1, 5)
            .filter { settings.getString("server_name$it", "")!! == serverName }
            .map { settings.getString("server$it", "") }
            .first()
        if (serverAddress.isNullOrEmpty()) {
            alert(this, "Server address is empty")
            return
        }

        var serverProtocol = IntRange(1, 5)
            .filter { settings.getString("server_name$it", "")!! == serverName }
            .map { settings.getString("server_protocol$it", "") }
            .first()
        if (serverProtocol.isNullOrEmpty()) {
            alert(this, "Server protocol is empty")
            return
        }

        var port = 60001
        val serverAddressParts = serverAddress.split(':')
        if (serverAddressParts.size == 2) {
            serverAddress = serverAddressParts[0]
            val mayBePort = serverAddressParts[1].toIntOrNull()
            if (mayBePort != null) {
                port = mayBePort
            } else {
                alert(this, "wrong serverAddress")
                return
            }
        }

        SensorsFragment.useV2 = serverProtocol == "2"
        if (SensorsFragment.useV2)
            SmartHomeServiceV2.setupServer(serverAddress, port)
        else
            SmartHomeService.setupServer(serverAddress, port)
        if (refreshData)
          refreshFragment()
    }

    override fun onCreateOptionsMenu(menu: Menu): Boolean {
        // Inflate the menu; this adds items to the action bar if it is present.
        menuInflater.inflate(R.menu.main, menu)
        return true
    }

    private fun refreshFragment() {
        val fragment = supportFragmentManager.findFragmentById(R.id.fragment_container)
        if (fragment != null && fragment is ISmartHomeData) {
            (fragment as ISmartHomeData).refresh()
        }
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        // Handle action bar item clicks here. The action bar will
        // automatically handle clicks on the Home/Up button, so long
        // as you specify a parent activity in AndroidManifest.xml.

        when (item.itemId) {
            R.id.action_settings -> {
                val intent = Intent(this, SettingsActivity::class.java)
                intent.putExtra("code", SETTINGS)
                mActivityResultLauncher!!.launch(intent)
                return true
            }
            R.id.action_refresh -> {
                refreshFragment()
                return true
            }
            R.id.action_filters -> {
                val intent = Intent(this, FiltersActivity::class.java)
                intent.putExtra("code", FILTERS)
                intent.putExtra("data", filters)
                mActivityResultLauncher!!.launch(intent)
                return true
            }
        }

        return super.onOptionsItemSelected(item)
    }

    private fun openPage(page: Int) {
        pageId = when {
            page < 0 -> {
                PAGE_MAX
            }
            page > PAGE_MAX -> {
                0
            }
            else -> {
                page
            }
        }
        when (pageId) {
            0 -> supportFragmentManager.beginTransaction().replace(R.id.fragment_container, HomePageFragment()).commit()
            1 -> supportFragmentManager.beginTransaction().replace(R.id.fragment_container, EnvSensorsFragment(this)).commit()
            2 -> supportFragmentManager.beginTransaction().replace(R.id.fragment_container, WatSensorsFragment(this)).commit()
            3 -> supportFragmentManager.beginTransaction().replace(R.id.fragment_container, EleSensorsFragment(this)).commit()
        }
    }

    override fun onNavigationItemSelected(item: MenuItem): Boolean {
        // Handle navigation view item clicks here.

        when (item.itemId) {
            R.id.nav_home -> openPage(0)
            R.id.nav_env -> openPage(1)
            R.id.nav_wat -> openPage(2)
            R.id.nav_ele -> openPage(3)
        }

        val drawer = findViewById<DrawerLayout>(R.id.drawer_layout)
        drawer.closeDrawer(GravityCompat.START)
        return true
    }

    override fun onActivityResult(result: ActivityResult) {
        val requestCode = result.data!!.getIntExtra("code", -2)
        if (result.resultCode == Activity.RESULT_OK) {
            when (requestCode) {
                SETTINGS -> {
                    try {
                        updateServer(true)
                    } catch (e: Exception) {
                        alert(this, e.message)
                    }
                }
                FILTERS -> {
                    if (result.data != null) {
                        filters = result.data!!.getParcelableExtra("data")!!
                        refreshFragment()
                    }
                }
            }
        }
    }

    override fun getData(): FiltersActivity.Data {
        return filters
    }

    override fun onItemSelected(parent: AdapterView<*>?, view: View?, position: Int, id: Long) {
        if (!disableRefresh)
            refresh(true)
        else
            disableRefresh = false
    }

    override fun onNothingSelected(parent: AdapterView<*>?) {
    }
}
