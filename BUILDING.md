# REMOTE BUILD (GITHUB)

RefindPlus can be built by leveraging GitHub's `Workflow Artefact` creation and storage capabilities.\
A GitHub `Workflow Action` is included in this repository to facilitate this.

- Navigate to https://github.com/RefindPlusRepo/RefindPlus and fork the repository.
- Navigate to `https://github.com/YOUR_GITHUB_USERNAME_GOES_HERE/RefindPlus.git`.
  - Enable Github Actions
    - Click on the `Settings` navigation option
    - Select the `Actions` configuration option
    - Allow actions under the `Actions` tab
  - Trigger Github Action
    - Click on the `Actions` navigation option.
    - Select the `Build Artefacts` workflow option.
    - Trigger the workflow using the dropdown menu option.

> [!NOTE]
>
> Replace `YOUR_GITHUB_USERNAME_GOES_HERE` above with your actual GitHub User Name.

Once the workflow run is completed, click on the action instance displayed and look for `"Artifacts"` near the bottom of the page for available builds to download.

> [!TIP]
>
> You might want to `disable Github Actions` on your fork after downloading the artefact builds, and only enable the workflow action when needed, to prevent periodic RefindPlusRepo-specific workflows from running on your fork.

> [!TIP]
>
> If your repository fork has `diverged from RefindPlusRepo`, refer to the [REPOSITORY SYNC](https://github.com/RefindPlusRepo/RefindPlus/blob/GOPFix/BUILDING.md#repository-sync) section for sync options.


<br><br>

---

<br><br>

# LOCAL BUILD (DOCKER)

RefindPlus can be built on any operating system environment that supports Docker virtualisation. A Docker image has been created by a third party developer and is available on the DockerHub website (https://hub.docker.com/r/xaionaro2/edk2-builder).

Please refer to that project's repository (https://github.com/xaionaro/edk2-builder-docker) for details and support on this option.

<br><br>

---

<br><br>

> [!CAUTION]
>
> The process outlined below *HAS NOT* been verified on Mac OS 26.x Tahoe/Newer.

# LOCAL BUILD (MAC OS)

## Python

The build process requires Python 2 but Python was essentially removed from Mac OS in 12.x Monterey.\
If running this version of Mac OS or newer, download and install Python 2.7.18 from the Python website (https://www.python.org/downloads/release/python-2718).

> [!TIP]
>
> Python 2 is available by default on Mac OS 11.x Big Sur and older.

## Xcode

### Base Installation

Download the version of Xcode for your Mac OS version from the Mac App Store and install.\
The third-party maintained XcodeReleases website (https://xcodereleases.com) provides convenient links to Xcode packages on Apple's servers.

### Commandline Tools Installation

After installing Xcode, you will need to additionally install its commandline tools.\
To do this, at a Terminal prompt, enter:

```
$ xcode-select --install
```

## HomeBrew

### Background

While Xcode provides a full development environment as well as a suite of different utilities, it does not provide all the tools required for TianoCore EDK II development as required to build RefindPlus on Mac OS natively.

This guide focuses on using HomeBrew to provide the required tools but equivalent steps can be taken in MacPorts and Fink. These may offer better support for older versions of Mac OS.\
Substitute equivalent commands in as required.

You will find HomeBrew installation instructions on the project website (https://brew.sh)

### Install Build Assembler

The assembler used for TianoCore EDK II is the Netwide Assembler (NASM).

```
$ brew install nasm && brew upgrade nasm
```

### Install ACPI Compiler

ACPICA is required to compile code in ACPI Source Language for TianoCore EDK II firmware builds.

```
$ brew install acpica && brew upgrade acpica
```

### Install Image Converter

The ocmtoc utility converts the Mach-O image format generated on Mac OS to the PE/COFF format required by the UEFI specifications.

```
$ brew uninstall mtoc && brew install ocmtoc && brew upgrade ocmtoc
```

> [!NOTE]
>
> `ocmtoc` is only available as an HomeBrew package on Mac OS 11.x Big Sur and newer but pre-built `ocmtoc` files can be used on Mac OS versions back to 10.9 Mavericks.
>
> Pre-built files can be found here: https://github.com/acidanthera/ocmtoc/releases.

## Prepare RefindPlus Environment

### Fork the RefindPlus Repository

Navigate to `https://github.com/RefindPlusRepo/RefindPlus` and fork the repository.

### Clone the Forked RefindPlus Repository

In Terminal, clone the forked `RefindPlus` repository into a `RefindPlus/Working` folder under your `Documents` directory as follows:

```
$ mkdir -p ~/Documents/RefindPlus && cd ~/Documents/RefindPlus
$ git clone https://github.com/YOUR_GITHUB_USERNAME_GOES_HERE/RefindPlus.git Working
$ cd ~/Documents/RefindPlus/Working && git checkout GOPFix
$ git remote add upstream https://github.com/RefindPlusRepo/RefindPlus.git
```

> [!NOTE]
>
> Replace `YOUR_GITHUB_USERNAME_GOES_HERE` above with your actual GitHub User Name.

Your local RefindPlus repository will be under `Documents/RefindPlus/Working`.

## Prepare UDK2018 Environment

### Fork the RefindPlusUDK Repository

Navigate to `https://github.com/RefindPlusRepo/RefindPlusUDK` and fork the repository

### Clone the Forked RefindPlusUDK Repository

In Terminal, clone the forked `RefindPlusUDK` repository into a `RefindPlus/edk2` folder under your `Documents` directory as follows:

```
$ mkdir -p ~/Documents/RefindPlus && cd ~/Documents/RefindPlus
$ git clone https://github.com/YOUR_GITHUB_USERNAME_GOES_HERE/RefindPlusUDK.git edk2
$ cd ~/Documents/RefindPlus/edk2 && git checkout rudk
$ git remote add upstream https://github.com/RefindPlusRepo/RefindPlusUDK.git
```

> [!NOTE]
>
> Replace `YOUR_GITHUB_USERNAME_GOES_HERE` above with your actual GitHub User Name.

Your local RefindPlusUDK repository will be under `Documents/RefindPlus/edk2`.

## Run Build Script

- Navigate to your `/Documents/RefindPlus/edk2/000-BuildScript` folder in the Finder.
- Separately, open a new Terminal window.
  - Always use a new Terminal window when building.
- Type `chmod +x` in Terminal, add a space, then drag the `RefindPlusBuilder.sh` file onto the Terminal window and press `Enter`.
- Type `bash` in Terminal, add a space, then drag the `RefindPlusBuilder.sh` file to the Terminal window again and press `Enter`.
  - Enter a space followed by a branch name to the end of the line (if you want to build on that branch) before pressing `Enter`.
  - If nothing is entered, the script will build on the default `GOPFix` branch.
  - The "chmod +x" step is typically only required the first time the script file is ever run.

# REPOSITORY SYNC

If some time has passed since your last build or since you initially created your repositories, you will need to ensure your repositories are aligned with the source repositories to incorporate updates added in the intervening period.

## OPTION 1: Scripted Sync (Recommended)

> [!NOTE]
>
> This option requires a previously prepared [RefindPlusUDK environment](https://github.com/RefindPlusRepo/RefindPlus/blob/GOPFix/BUILDING.md#prepare-udk2018-environment).

- Navigate to your `/Documents/RefindPlus/edk2/000-BuildScript` folder in the Finder.
- Separately, open a new Terminal window.
  - A new Terminal window is best for syncing.
- Type `chmod +x`, add a space, then drag the `RepoUpdater.sh` file onto the Terminal window and press `Enter`.
- Type `bash`, add a space, then drag the `RepoUpdater.sh` file to the Terminal window again and press `Enter`.
  - The "chmod +x" step is typically only required the first time the script file is ever run

> [!TIP]
>
> If you get an error after running the script, try running it again as subsequent runs should realign things.
>
> If the script still fails after a third attempt, try the manual sync steps outlined below instead.

## OPTION 2: Manual Sync

### Sync RefindPlus Manually

```
$ cd ~/Documents/RefindPlus/Working && git checkout GOPFix
$ (git remote get-url upstream 2>/dev/null | grep -q "https://github.com/RefindPlusRepo/RefindPlus.git") || (git remote remove upstream && git remote add upstream https://github.com/RefindPlusRepo/RefindPlus.git 2>/dev/null)
$ git reset --hard a2cc87f019c4de3a1237e2dc23f432c27cec5ec6
$ git push origin HEAD -f && git pull upstream GOPFix
$ git push
```

### Sync RefindPlusUDK Manually

```
$ cd ~/Documents/RefindPlus/edk2 && git checkout rudk
$ (git remote get-url upstream 2>/dev/null | grep -q "https://github.com/RefindPlusRepo/RefindPlusUDK.git") || (git remote remove upstream && git remote add upstream https://github.com/RefindPlusRepo/RefindPlusUDK.git 2>/dev/null)
$ git reset --hard a94082b4e5e42a1cfdcbab0516f9ecdbb596d201
$ git push origin HEAD -f && git pull upstream rudk
$ git push
```

## OPTION 3: GitHub Sync

GitHub includes an interface for syncing forks.\
While, unlike Option 3, Options 1 and 2 will always leave your fork with a clean history consistent with the source repositories, some may find the GitHub interface convenient.
