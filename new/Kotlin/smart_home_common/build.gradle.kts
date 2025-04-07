plugins {
    kotlin("jvm") version "2.1.10"
}

group = "org.example"
version = "0.1"

repositories {
    mavenCentral()
}

dependencies {
    api("org.apache.commons:commons-compress:1.27.1")
    testImplementation(kotlin("test"))
}

tasks.test {
    useJUnitPlatform()
}

kotlin {
    jvmToolchain(23)
}

tasks.jar {
    from(sourceSets.main.get().output)
}
