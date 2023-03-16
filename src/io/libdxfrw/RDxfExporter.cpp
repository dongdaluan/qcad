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

#include <QFileInfo>
#include <QFont>

#include "RArcEntity.h"
#include "RAttributeEntity.h"
#include "RBlockReferenceEntity.h"
#include "RCircleEntity.h"
#include "RColor.h"
#include "RDimAlignedEntity.h"
#include "RDimAngular2LEntity.h"
#include "RDimAngular3PEntity.h"
#include "RDimDiametricEntity.h"
#include "RDimOrdinateEntity.h"
#include "RDimRadialEntity.h"
#include "RDimStyle.h"
#include "RDimensionEntity.h"
#include "RDxfExporter.h"
#include "REllipseEntity.h"
#include "RHatchEntity.h"
#include "RImageEntity.h"
#include "RLeaderEntity.h"
#include "RLineEntity.h"
#include "RPointEntity.h"
#include "RPolylineEntity.h"
#include "RRayEntity.h"
#include "RSettings.h"
#include "RSolidEntity.h"
#include "RSplineEntity.h"
#include "RStorage.h"
#include "RTextEntity.h"
#include "RTraceEntity.h"
#include "RXLineEntity.h"

extern double dxfColors[][3];

DRW_Text::VAlign VAlign2dxf(RS::VAlign v)
{
	switch (v) {
	case RS::VAlignBase:
		return DRW_Text::VBaseLine;
	case RS::VAlignTop:
		return DRW_Text::VTop;
	case RS::VAlignMiddle:
		return DRW_Text::VMiddle;
	case RS::VAlignBottom:
		return DRW_Text::VBottom;
	}
	return DRW_Text::VBaseLine;
}

RDxfExporter::RDxfExporter(RDocument& document,
	RMessageHandler* messageHandler,
	RProgressHandler* progressHandler)
	: RFileExporter(document, messageHandler, progressHandler)
{

}

QString RDxfExporter::getCorrectedFileName(const QString& fileName, const QString& nameFilter) {
	Q_UNUSED(nameFilter)

		QString ret = fileName;
	QString ext = QFileInfo(ret).suffix().toLower();

	if (ext != "dxf") {
		ret += ".dxf";
	}

	return ret;
}

bool RDxfExporter::exportFile(const QString& fileName, const QString& nameFilter, bool setFileName) {
	//qDebug() << "RDxfExporter::exportFile";
	DRW::Version v = DRW::AC1018;
	if (setFileName) {
		document->setFileName(fileName);
	}
	bool binary = nameFilter.contains("Binary");
	bool dwg = nameFilter.contains("DWG");

	bool success = false;
	if (dwg) {
		qWarning() << "not support dwg";
		return false;
	}
	else {
		_dxfW = new dxfRW(fileName.toLocal8Bit());
		success = _dxfW->write(this, v, binary);
		delete _dxfW;
	}
	if (!success || QFileInfo(fileName).exists() == false) {
		return false;
	}

	return true;
}

void RDxfExporter::writeHeader(DRW_Header& data) {
	QSharedPointer<RDimStyle> dimStyle = document->queryDimStyleDirect();
	dimStyle->updateDocumentVariables();

	auto vars = document->getVariables();
	for (QString var : vars) {
		if (!var.startsWith("$"))
			continue;
		auto val = document->getVariable(var);
		switch (val.type()) {
		case QVariant::String:
			data.addStr(var.toLocal8Bit().constData(), var.toStdString(), 0);
			break;
		case QVariant::Int:
			data.addInt(var.toLocal8Bit().constData(), var.toInt(), 0);
			break;
		case QVariant::Double:
			data.addDouble(var.toLocal8Bit().constData(), var.toDouble(), 0);
			break;
		default:
			break;
		}
	}
	data.addComment("DXF file generated by QCAD");
}
void RDxfExporter::writeBlocks() {
	QStringList blockNames = RS::toList(document->getBlockNames());
	for (int i = 0; i < blockNames.size(); ++i) {
		QSharedPointer<RBlock> blk = document->queryBlock(blockNames[i]);
		if (blk.isNull())
			continue;

		DRW_Block block;
		block.layer = blk->getLayoutName().toUtf8().constData();
		block.name = blk->getName().toUtf8().constData();
		auto origin = blk->getOrigin();
		block.basePoint.x = origin.x;
		block.basePoint.y = origin.y;
		block.basePoint.z = origin.z;
		_dxfW->writeBlock(&block);

		auto entityIds = RS::toList(document->queryBlockEntities(blk->getId()));
		for (int j = 0; j < entityIds.size(); ++j) {
			QSharedPointer<REntity> entity = document->queryEntity(entityIds[j]);
			if (entity.isNull())
				continue;

			writeEntity(entity);
		}
	}
}
void RDxfExporter::writeBlockRecords() {
	QStringList blockNames = RS::toList(document->getBlockNames());
	for (int i = 0; i < blockNames.size(); ++i) {
		_dxfW->writeBlockRecord(blockNames[i].toUtf8().constData());
	}
}
void RDxfExporter::writeEntities() {

}
void RDxfExporter::writeLTypes() {
	QStringList lts = RS::toList(document->getLinetypeNames());
	for (int i = 0; i < lts.size(); i++) {
		QSharedPointer<RLinetype> lt = document->queryLinetype(lts[i]);
		if (!lt.isNull()) {
			DRW_LType lType;
			lType.name = lt->getName().toUtf8().constData();
			lType.desc = lt->getDescription().toUtf8().constData();

			auto pat = lt->getPattern();
			auto dashes = pat.getPattern();
			for (int j = 0; j < dashes.size(); j++) {
				lType.path.emplace_back(dashes.at(j));
			}

			_dxfW->writeLineType(&lType);
		}
	}
}

void RDxfExporter::writeLayers() {
	QStringList layerNames = RS::toList(document->getLayerNames());
	for (int i = 0; i < layerNames.size(); i++)
	{
		DRW_Layer layer;
		layer.name = layerNames[i].toUtf8().constData();
		QSharedPointer<RLayer> lay = document->queryLayer(layerNames[i]);
		auto color = lay->getColor();
		layer.color24 = RDxfServices::colorToNumber24(color);
		layer.color = RDxfServices::colorToNumber(color, dxfColors);
		layer.lWeight = DRW_LW_Conv::dxfInt2lineWidth(lay->getLineweight());
		auto ltype = document->queryLinetype(lay->getLinetypeId());
		if (!ltype.isNull()) {
			layer.lineType = ltype->getName().toUtf8().constData();
		}
		_dxfW->writeLayer(&layer);
	}
}
void RDxfExporter::writeTextstyles() {
	auto allEntities = RS::toList(document->queryAllEntities(false, true));
	for (auto it = allEntities.begin(); it != allEntities.end(); it++) {
		auto e = document->queryEntity(*it);
		if (e->getType() == RS::EntityText)
		{
			auto txt = e.dynamicCast<RTextEntity>();
			if (txt.isNull())
				continue;
			QString styleName = txt->getData().getStyleName();
			QString bigFont = txt->getData().getBigFontFile();
			QString fontFile = txt->getData().getFontFile();
			QString fontName = txt->getData().getFontName();
			double xScale = txt->getData().getXScale();

			styleName = addTextStyle(styleName, fontName, fontFile, bigFont, xScale);
			txt->getData().setStyleName(styleName);
		}
	}

	for (auto it = _textStyles.begin(); it != _textStyles.end(); it++) {
		DRW_Textstyle style;
		style.bigFont = it->bigFont.toUtf8().constData();
		style.font = it->fontFile.toUtf8().constData();
		style.fontName = it->font.toUtf8().constData();
		style.width = it->width;
		style.name = it->name.toUtf8().constData();
		_dxfW->writeTextstyle(&style);
	}
}
void RDxfExporter::writeVports() {
}
void RDxfExporter::writeDimstyles() {
}
void RDxfExporter::writeAppId() {
}

void RDxfExporter::writeEntity(const QSharedPointer<REntity>& entity)
{
	switch (entity->getType()) {
	case RS::EntityPoint:
		writePoint(entity.dynamicCast<RPointEntity>());
		break;
	case RS::EntityLine:
		writeLine(entity.dynamicCast<RLineEntity>());
		break;
	case RS::EntityCircle:
		writeCircle(entity.dynamicCast<RCircleEntity>());
		break;
	case RS::EntityArc:
		writeArc(entity.dynamicCast<RArcEntity>());
		break;
	case RS::EntityEllipse:
		writeEllipse(entity.dynamicCast<REllipseEntity>());
		break;
	case RS::EntityPolyline:
		writePolyline(entity.dynamicCast<RPolylineEntity>());
		break;
	case RS::EntitySpline:
		writeSpline(entity.dynamicCast<RSplineEntity>());
		break;
	case RS::EntityText:
		writeText(entity.dynamicCast<RTextEntity>());
		break;
	case RS::EntityHatch:
		writeHatch(entity.dynamicCast<RHatchEntity>());
		break;
	case RS::EntityImage:
		writeImage(entity.dynamicCast<RImageEntity>());
		break;
	default:
		break;
	}
}

void RDxfExporter::writePoint(const QSharedPointer<RPointEntity>& entity) {
}
void RDxfExporter::writeLine(const QSharedPointer<RLineEntity>& entity) {
	DRW_Line line;
	setEntity(&line, entity.dynamicCast<REntity>());

	auto s = entity->getStartPoint();
	auto e = entity->getEndPoint();

	line.basePoint = DRW_Coord(s.x, s.y, s.z);
	line.secPoint = DRW_Coord(e.x, e.y, e.z);
	_dxfW->writeLine(&line);
}
void RDxfExporter::writeCircle(const QSharedPointer<RCircleEntity>& entity) {
}
void RDxfExporter::writeArc(const QSharedPointer<RArcEntity>& entity) {
}
void RDxfExporter::writeEllipse(const QSharedPointer<REllipseEntity>& entity) {
}
void RDxfExporter::writePolyline(const QSharedPointer<RPolylineEntity>& entity) {
	DRW_LWPolyline polyline;
	setEntity(&polyline, entity.dynamicCast<REntity>());

	polyline.flags = entity->isClosed() ? 1 : 0;
	if (entity->getPolylineGen()) {
		polyline.flags |= 0x80;
	}
	auto cnt = entity->countVertices();
	polyline.vertexnum = cnt;
	for (int i = 0; i < cnt; i++) {
		auto v = entity->getVertexAt(i);
		DRW_Vertex2D* vert = new DRW_Vertex2D(v.x, v.y, v.z);
		polyline.vertlist.emplace_back(vert);
	}

	_dxfW->writeLWPolyline(&polyline);
}
void RDxfExporter::writeSpline(const QSharedPointer<RSplineEntity>& entity) {
}
void RDxfExporter::writeText(const QSharedPointer<RTextEntity>& entity) {
	if (entity->getData().isSimple()) {
		DRW_Text text;
		setEntity(&text, entity.dynamicCast<REntity>());
		text.height = entity->getTextHeight();
		text.text = entity->getEscapedText().toUtf8().constData();
		text.angle = entity->getAngle();
		text.alignH = (DRW_Text::HAlign)entity->getHAlign();
		text.alignV = VAlign2dxf(entity->getVAlign());
		text.widthscale = entity->getXScale();
		text.style = entity->getData().getStyleName().toUtf8().constData();

		auto p = entity->getPosition();
		auto a = entity->getAlignmentPoint();
		text.basePoint = DRW_Coord(a.x, a.y, a.z);
		text.secPoint = DRW_Coord(p.x, p.y, p.z);

		_dxfW->writeText(&text);
	}
	else {
		DRW_MText text;
		setEntity(&text, entity.dynamicCast<REntity>());
		text.height = entity->getTextHeight();
		text.text = entity->getEscapedText().toUtf8().constData();
		text.angle = entity->getAngle();
		text.interlin = entity->getLineSpacingFactor();
		auto dir = entity->getDrawingDirection();
		switch (dir) {
		case RS::TopToBottom:
			text.drawingDirection = 3;
			break;
		case RS::LeftToRight:
			text.drawingDirection = 1;
			break;
		default:
			text.drawingDirection = 5;
			break;
		}
		auto lss = entity->getLineSpacingStyle();
		switch (lss) {
		case RS::AtLeast:
			text.lineSpacingStyle = 1;
			break;
		case RS::Exact:
			text.lineSpacingStyle = 2;
			break;
		}

		auto alignH = entity->getHAlign();
		auto alignV = entity->getVAlign();		
		if (alignH == RS::HAlignLeft && alignV == RS::VAlignTop)
			text.attachmentPoint = 1;
		else if (alignH == RS::HAlignCenter && alignV == RS::VAlignTop)
			text.attachmentPoint = 2;
		else if (alignH == RS::HAlignRight && alignV == RS::VAlignTop)
			text.attachmentPoint = 3;
		else if (alignH == RS::HAlignLeft && alignV == RS::VAlignMiddle)
			text.attachmentPoint = 4;
		else if (alignH == RS::HAlignCenter && alignV == RS::VAlignMiddle)
			text.attachmentPoint = 5;
		else if (alignH == RS::HAlignRight && alignV == RS::VAlignMiddle)
			text.attachmentPoint = 6;
		else if (alignH == RS::HAlignLeft && alignV == RS::VAlignBottom)
			text.attachmentPoint = 7;
		else if (alignH == RS::HAlignCenter && alignV == RS::VAlignBottom)
			text.attachmentPoint = 8;
		else if (alignH == RS::HAlignRight && alignV == RS::VAlignBottom)
			text.attachmentPoint = 9;
		else
			text.attachmentPoint = 5;

		auto p = entity->getPosition();
		auto a = entity->getAlignmentPoint();
		text.basePoint = DRW_Coord(a.x, a.y, a.z);
		text.secPoint = DRW_Coord(p.x, p.y, p.z);
		text.style = entity->getData().getStyleName().toUtf8().constData();
		text.widthscale = entity->getTextWidth();
		_dxfW->writeMText(&text);
	}
}
void RDxfExporter::writeHatch(const QSharedPointer<RHatchEntity>& entity) {
	DRW_Hatch hatch;
	setEntity(&hatch, entity.dynamicCast<REntity>());

	hatch.name = entity->getPatternName().toUtf8().constData();
	hatch.solid = entity->isSolid();
	hatch.angle = entity->getAngle();
	hatch.scale = entity->getScale();

	auto bp = entity->getOriginPoint();
	hatch.basePoint = DRW_Coord(bp.x, bp.y, bp.z);

	int cnt = entity->getLoopCount();
	for (int i = 0; i < cnt; i++) {
		auto loops = entity->getLoopBoundary(i);
		auto hatLoop = new DRW_HatchLoop(1);
		hatLoop->numedges = loops.size();
		for (int j = 0; j < loops.size(); j++) {
			auto loop = loops.at(j);
			auto type = loop->getShapeType();
			Q_ASSERT(type == RShape::Line);
			if (type == RShape::Line)
			{
				auto pp = loop.dynamicCast<RLine>();
				auto pl = new DRW_Line();

				{
					auto v = pp->getStartPoint();
					pl->basePoint.x = v.x;
					pl->basePoint.y = v.y;
					pl->basePoint.z = v.z;
				}
				{
					auto v = pp->getEndPoint();
					pl->secPoint.x = v.x;
					pl->secPoint.y = v.y;
					pl->secPoint.z = v.z;
				}

				hatLoop->objlist.emplace_back(pl);
			}
		}
		hatch.looplist.emplace_back(hatLoop);
	}

	auto pattern = entity->getCustomPattern();
	auto lines = pattern.getPatternLines();
	hatch.deflinesnum = lines.size();
	for (int i = 0; i < hatch.deflinesnum; i++)
	{
		auto line = lines.at(i);

		DRW_HatchDefine* define = new DRW_HatchDefine();

		define->angle = RMath::rad2deg(line.angle);
		define->origin = DRW_Coord(line.basePoint.x, line.basePoint.y, line.basePoint.z);

		double sinDelta = sin(line.angle);
		double cosDelta = cos(line.angle);

		define->delta.x = cosDelta * line.offset.x - sinDelta * line.offset.y;
		define->delta.y = sinDelta * line.offset.x + cosDelta * line.offset.y;

		define->segmentsnum = line.dashes.size();
		for (int j = 0; j < define->segmentsnum; j++) {
			define->segments.emplace_back(line.dashes.at(j));
		}

		hatch.defines.emplace_back(define);
	}

	_dxfW->writeHatch(&hatch);
}
void RDxfExporter::writeImage(const QSharedPointer<RImageEntity>& entity) {
}
void RDxfExporter::setEntity(DRW_Entity* e, QSharedPointer<REntity> entity) {
	auto layer = document->queryLayer(entity->getLayerId());
	if (!layer.isNull())
		e->layer = layer->getName().toUtf8().constData();
	auto lineType = document->queryLinetype(entity->getLinetypeId());
	if (!lineType.isNull())
		e->lineType = lineType->getName().toUtf8().constData();
	e->lWeight = DRW_LW_Conv::dxfInt2lineWidth(entity->getLineweight());
	auto color = entity->getColor();
	e->color24 = RDxfServices::colorToNumber24(color);
	e->color = RDxfServices::colorToNumber(color, dxfColors);
	e->ltypeScale = entity->getLinetypeScale();

	RBlock::Id paperSpaceBlockId = document->getBlockId("*Paper_Space");
	if (entity->getBlockId() == paperSpaceBlockId) {
		e->space = DRW::PaperSpace;
	}
}

QString RDxfExporter::addTextStyle(QString name, QString fontName, QString fontFile, QString bigFont, double xScale)
{
	auto it = _textStyles.begin();
	for (; it != _textStyles.end(); it++) {
		if (it->bigFont == bigFont && it->font == fontName && it->fontFile == fontFile && xScale == xScale)
			break;
	}
	if (name.isEmpty())
		name = "Unknown";
	if (it == _textStyles.end()) {
		int idx = 0;
		QString orgName = name;
		do
		{
			if (!_textStyles.contains(name)) {
				break;
			}
			idx++;
			name = orgName + QString::number(idx);
		} while (true);
		TextStyle textStyle;
		textStyle.bigFont = bigFont;
		textStyle.font = fontName;
		textStyle.fontFile = fontFile;
		textStyle.width = xScale;
		textStyle.name = name;
		_textStyles.insert(name, textStyle);
	}
	else {
		name = it->name;
	}
	return name;
}