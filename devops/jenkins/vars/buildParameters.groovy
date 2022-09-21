def branch() {
    return "master"
}

def version() {
    return "1.14.8"
}

def linuxAgents() {
    return ["cloud-ubuntu-20", "bld-agent"]
}

def win32Agents() {
    return ["win7_x32"]
}

def win64Agents() {
    return ["cloud-windows-2012", "win10_x64_veil_guest_agent"]
}