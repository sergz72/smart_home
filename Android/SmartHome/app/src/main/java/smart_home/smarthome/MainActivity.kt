package smart_home.smarthome

import android.app.Activity
import android.app.AlertDialog
import android.content.Intent
import android.gesture.Gesture
import android.gesture.GestureOverlayView
import android.graphics.Point
import android.os.Bundle
import android.support.design.widget.NavigationView
import android.support.v4.view.GravityCompat
import android.support.v4.widget.DrawerLayout
import android.support.v7.app.ActionBarDrawerToggle
import android.support.v7.app.AppCompatActivity
import android.support.v7.widget.Toolbar
import android.view.*

class MainActivity : AppCompatActivity(), NavigationView.OnNavigationItemSelectedListener, GestureOverlayView.OnGesturePerformedListener {
    companion object {
        const val PREFS_NAME = "SmartHome"
        private const val SETTINGS = 1
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

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        val toolbar = findViewById<View>(R.id.toolbar) as Toolbar
        setSupportActionBar(toolbar)

        val drawer = findViewById<View>(R.id.drawer_layout) as DrawerLayout
        val toggle = ActionBarDrawerToggle(
                this, drawer, toolbar, R.string.navigation_drawer_open, R.string.navigation_drawer_close)
        drawer.addDrawerListener(toggle)
        toggle.syncState()

        val navigationView = findViewById<View>(R.id.nav_view) as NavigationView
        navigationView.setNavigationItemSelectedListener(this)

        val gestureView = findViewById<View>(R.id.gestures) as GestureOverlayView
        gestureView.addOnGesturePerformedListener(this)

        SmartHomeService.setupKey(resources.openRawResource(R.raw.key))

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
        val drawer = findViewById<View>(R.id.drawer_layout) as DrawerLayout
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
                val fragment = supportFragmentManager.findFragmentById(R.id.fragment_container)
                if (fragment != null && fragment is ISmartHomeData) {
                    (fragment as ISmartHomeData).refresh()
                }
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
            1 -> supportFragmentManager.beginTransaction().replace(R.id.fragment_container, EnvSensorsFragment()).commit()
            2 -> supportFragmentManager.beginTransaction().replace(R.id.fragment_container, EleSensorsFragment()).commit()
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

        val drawer = findViewById<View>(R.id.drawer_layout) as DrawerLayout
        drawer.closeDrawer(GravityCompat.START)
        return true
    }

    override fun onGesturePerformed(overlay: GestureOverlayView, gesture: Gesture) {
        val strokes = gesture.strokes
        if (strokes.size == 1) {
            val display = windowManager.defaultDisplay
            val size = Point()
            display.getSize(size)
            val width = size.x
            val height = size.y
            val gestureWidth = gesture.boundingBox.width()
            if (gesture.length <= gestureWidth * 1.3 && gestureWidth >= width / 2 && gesture.boundingBox.height() <= height / 10) {
                val points = strokes[0].points
                val startX = points[0]
                val endX = points[points.size - 2]
                if (startX < endX) {
                    openPage(pageId - 1)
                } else {
                    openPage(pageId + 1)
                }
            }
        }
    }

    override fun onActivityResult(requestCodeIn: Int, resultCode: Int, data: Intent?) {
        var requestCode = requestCodeIn
        requestCode = requestCode and 0xFFFF
        if (requestCode == SETTINGS && resultCode == Activity.RESULT_OK) {
            try {
                updateServer()
            } catch (e: Exception) {
                alert(this, e.message)
            }
        }
    }
}
