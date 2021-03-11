package smart_home.smarthome

import android.app.Activity
import android.app.AlertDialog
import android.content.Intent
import android.gesture.Gesture
import android.gesture.GestureOverlayView
import android.graphics.Point
import android.os.Bundle
import android.view.Menu
import android.view.MenuItem
import androidx.appcompat.app.ActionBarDrawerToggle
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.widget.Toolbar
import androidx.core.view.GravityCompat
import androidx.drawerlayout.widget.DrawerLayout
import com.google.android.material.navigation.NavigationView

class MainActivity : AppCompatActivity(), NavigationView.OnNavigationItemSelectedListener, IGraphParameters {
    companion object {
        const val PREFS_NAME = "SmartHome"
        private const val SETTINGS = 1
        private const val FILTERS  = 2
        private const val PAGE_MAX = 2

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

        SmartHomeService.setupKeyAndContext(resources.openRawResource(R.raw.key), this)

        try {
            updateServer()
            openPage(0)
        } catch (e: Exception) {
            alert(this, e.message)
        }
    }

    private fun updateServer() {
        val settings = getSharedPreferences(PREFS_NAME, 0)
        val name = settings.getString("server_name", "localhost")!!

        SmartHomeService.setupServer(name, 60001)
    }

    override fun onBackPressed() {
        val drawer = findViewById<DrawerLayout>(R.id.drawer_layout)
        if (drawer.isDrawerOpen(GravityCompat.START)) {
            drawer.closeDrawer(GravityCompat.START)
        } else {
            super.onBackPressed()
        }
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
        val id = item.itemId

        when (id) {
            R.id.action_settings -> {
                val intent = Intent(this, SettingsActivity::class.java)
                startActivityForResult(intent, SETTINGS)
                return true
            }
            R.id.action_refresh -> {
                refreshFragment()
                return true
            }
            R.id.action_filters -> {
                val intent = Intent(this, FiltersActivity::class.java)
                intent.putExtra("data", filters)
                startActivityForResult(intent, FILTERS)
                return true
            }
        }

        return super.onOptionsItemSelected(item)
    }

    private fun openPage(page: Int) {
        if (page < 0) {
            pageId = PAGE_MAX
        } else if (page > PAGE_MAX) {
            pageId = 0
        } else {
            pageId = page
        }
        when (pageId) {
            0 -> supportFragmentManager.beginTransaction().replace(R.id.fragment_container, HomePageFragment()).commit()
            1 -> supportFragmentManager.beginTransaction().replace(R.id.fragment_container, EnvSensorsFragment(this)).commit()
            2 -> supportFragmentManager.beginTransaction().replace(R.id.fragment_container, EleSensorsFragment(this)).commit()
        }
    }

    override fun onNavigationItemSelected(item: MenuItem): Boolean {
        // Handle navigation view item clicks here.
        val id = item.itemId

        when (id) {
            R.id.nav_home -> openPage(0)
            R.id.nav_env -> openPage(1)
            R.id.nav_ele -> openPage(2)
        }

        val drawer = findViewById<DrawerLayout>(R.id.drawer_layout)
        drawer.closeDrawer(GravityCompat.START)
        return true
    }

    override fun onActivityResult(requestCodeIn: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCodeIn, resultCode, data)
        var requestCode = requestCodeIn
        requestCode = requestCode and 0xFFFF
        if (resultCode == Activity.RESULT_OK) {
        when (requestCode) {
            SETTINGS -> {
                try {
                    updateServer()
                } catch (e: Exception) {
                    alert(this, e.message)
                }
            }
            FILTERS -> {
                if (data != null) {
                    filters = data.getParcelableExtra("data")!!
                    refreshFragment()
                }
            }
        }
        }
    }

    override fun getData(): FiltersActivity.Data {
        return filters
    }
}
