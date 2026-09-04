# Compiling Flatpak
The supplied flatpak manifest io.github.CDDAHackJob.Cataclysm-Salvaged.json
will download and build Cataclysm: Salvaged from its release branch.  It
builds from the repository, not from your local checkout, so local edits are
not picked up unless you point the manifest at your own tree.

Building it requires flatpak-builder on Linux; the manifest cannot be built
from the MSYS2/MinGW environment used for the Windows game builds.

Check the flatpak documentation at http://docs.flatpak.org for
information on how to customize the manifest, build your own copies of
the app, and install or distribute them.
