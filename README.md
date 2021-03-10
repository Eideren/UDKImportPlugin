A heavily modified [UDKImportPlugin](https://github.com/Speedy37/UDKImportPlugin). 
This plugin converts UDK materials to UE4.

### Differences
- Works as a directory structure import instead of the per package thing.
- Removed the UDK-side tool
    + that tool was redundant, you can batch export all of your assets from UDK's interface and removing it reduces the time importing takes.
- Removed texture and obj import, UE4 already supports them
- Fixed and improved material, material instance and decal materials conversion and support
- Fixed reference paths within material
- Upgraded to Unreal Engine 4.26

To import maps please use the tool at https://github.com/Eideren/Cartographer

How to install
----------
1. In your Unreal Engine 4 project, create the `Plugins` folder and clone this project.
You should have something like this : `MyProject/Plugins/UDKImportPlugin`
2. Then right click on your .uproject file and "Generate Visual Studio Project" to force generation of valid Visual Studio project with the plugin inside.
3. Start your project. While starting the editor should build the plugin.
4. Activate the plugin via the Plugin Manager and restart the editor

How to use
----------
1. Export your materials from UDK into a specific folder
2. Inside the Unreal Engine editor, go to `File > UDKImport`
3. Point the path field to your folder and run

Tips
----
If you want to keep texture references you should replicate the directory structure that you setup when exporting through UDK inside of your UE4 project.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.