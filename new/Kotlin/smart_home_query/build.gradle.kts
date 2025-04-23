plugins {
    kotlin("jvm") version "2.1.20"
}

group = "com.sz.smart_home.query"
version = "0.1"

repositories {
    mavenCentral()
}

dependencies {
    runtimeOnly("org.apache.commons:commons-compress:1.27.1")
    api("org.jetbrains.kotlinx:kotlinx-coroutines-core:1.10.2")
    implementation(files("../smart_home_common/build/libs/smart_home_common-0.1.jar"))
    testImplementation(kotlin("test"))
}

tasks.test {
    useJUnitPlatform()
}

tasks.jar {
    manifest {
        attributes["Main-Class"] = "com.sz.smart_home.query.MainKt"
    }
    duplicatesStrategy = DuplicatesStrategy.EXCLUDE
    from(sourceSets.main.get().output)
    dependsOn(configurations.runtimeClasspath)
    from({
        configurations.runtimeClasspath.get().filter { it.name.endsWith("jar") }.map { zipTree(it) }
    })
}
