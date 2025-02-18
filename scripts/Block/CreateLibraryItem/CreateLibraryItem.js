/**
 * Copyright (c) 2011-2018 by Andrew Mustun. All rights reserved.
 * 
 * This file is part of the QCAD project.
 *
 * QCAD is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * QCAD is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with QCAD.
 */

include("scripts/Widgets/LibraryBrowser/LibraryBrowser.js");
include("scripts/File/File.js");
include("scripts/Block/Block.js");
include("scripts/Block/BlockDialog.js");

function CreateLibraryItem(guiAction) {
    Block.call(this, guiAction);

    this.referencePoint = undefined;
}

CreateLibraryItem.prototype = new Block();

CreateLibraryItem.State = {
    SettingPosition : 0
};

CreateLibraryItem.prototype.beginEvent = function() {
    Block.prototype.beginEvent.call(this);

    this.setState(CreateLibraryItem.State.SettingPosition);
};

CreateLibraryItem.prototype.setState = function(state) {
    Block.prototype.setState.call(this, state);

    this.setCrosshairCursor();
    this.getDocumentInterface().setClickMode(RAction.PickCoordinate);

    var trRefPoint = qsTr("Reference Point");
    this.setCommandPrompt(trRefPoint);
    this.setLeftMouseTip(trRefPoint);
    this.setRightMouseTip(EAction.trCancel);

    EAction.showSnapTools();
};

CreateLibraryItem.prototype.coordinateEvent = function(event) {
    var pos = event.getModelPosition();
    this.getDocumentInterface().setRelativeZero(pos);
    this.referencePoint = pos;

    var di = this.getDocumentInterface();
    var document = this.getDocument();

    var itemStorage = new RMemoryStorage();
    var itemSpatialIndex = createSpatialIndex();
    var itemDocument = new RDocument(itemStorage, itemSpatialIndex);
    itemDocument.setUnit(document.getUnit());
    var itemDocumentInterface = new RDocumentInterface(itemDocument);
    itemDocumentInterface.setNotifyListeners(false);

    var op = new RCopyOperation(pos.getNegated(), document);
    op.setText(this.getToolTitle());
    itemDocumentInterface.applyOperation(op);

    var filterStrings = RFileExporterRegistry.getFilterStrings();
    filterStrings = translateFilterStrings(filterStrings);

    // Last used path or user data location:
    var userPath = RSettings.getDataLocation() + QDir.separator + "libraries";
    var initialPath = RSettings.getStringValue("CreateLibraryItem/Path", userPath);
    var defaultNameFilter = RSettings.getStringValue("SaveAs/Filter", "");

    // Revert to the original default when the folder does not exist:
    var destDir = new QDir(initialPath);
    if (!destDir.exists()) {
        // The former default 'LaunchPath/libraries' will always be valid
        initialPath = RSettings.getLaunchPath() + QDir.separator + "libraries";
    }

    // Get filename by dialog:
    var res = File.getSaveFileName(
        EAction.getMainWindow(),
        qsTr("Save library item as..."),
        initialPath,
        filterStrings,
        defaultNameFilter);

    if (!isNull(res)) {
        // # Issue Fixed # The export may not be successful, a warning is handled
        var success = itemDocumentInterface.exportFile(res[0], res[1]);
        // With a successful export update the Library Browser path and the last used path:
        if (success) {
            var fi = new QFileInfo(res[0]);
            LibraryBrowser.showDirectory(fi.absolutePath());
            // Store last used path persistent:
            RSettings.setValue("CreateLibraryItem/Path", fi.absolutePath());
        }
    }

    itemDocumentInterface.destroy();
    this.terminate();
};

