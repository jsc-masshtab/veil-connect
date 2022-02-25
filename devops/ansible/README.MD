# Ansible roles for VeiL Connect
## Windows build-agent
### Prepare Windows machine
1. Run powershell script (as Administrator):
```
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
$url = "https://raw.githubusercontent.com/ansible/ansible/devel/examples/scripts/ConfigureRemotingForAnsible.ps1"
$file = "$env:temp\ConfigureRemotingForAnsible.ps1"
(New-Object -TypeName System.Net.WebClient).DownloadFile($url, $file)
powershell.exe -ExecutionPolicy ByPass -File $file

winrm enumerate winrm/config/Listener
winrm set winrm/config/service '@{AllowUnencrypted="true"}'
winrm set winrm/config/service/auth '@{Basic="true"}'
```
2. Define windows machine ip address in `inventory` file
3. Define login and password from windows machine in `windows_builder_play.yml` file
4. Run ansible playbook
```
sudo pip install ansible "pywinrm>=0.3.0"
ansible-playbook windows_builder_play.yml -i inventory
```