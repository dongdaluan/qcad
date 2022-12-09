#include <QFileInfo>
#include <QTextCodec>
#include <qendian.h>
#include "RShxFont.h"

struct BigFontTable
{
    uint16_t ch;
    uint32_t offset;
    uint16_t len;
};

RShxFont::RShxFont(const QString& fileName)
	: RFont(fileName)
{
    type = Unifont;
    drawMode = false;
    indexCount = 0;
    rangeCount = 0;
    scale = 1.;
    penX = 0;
    penY = 0;
}

bool RShxFont::load() {
	if (loaded) {
		return true;
	}

    loaded = true;
    QString path;

    // Search for the appropriate font if we only have the name of the font:
    if (!fileName.toLower().endsWith(".shx")) {
        QStringList fonts = RS::getFontList();
        for (int i = 0; i < fonts.size(); ++i) {
            QString p = fonts.at(i);
            if (QFileInfo(p).completeBaseName().toLower() == fileName.toLower()) {
                path = p;
                break;
            }
        }
    }

    // We have the full path of the font:
    else {
        path = fileName;
    }
    // No font paths found:
    if (path.isEmpty()) {
        qWarning() << "RShxFont::load: font not available.";
        return false;
    }
    // Open shx file:
    FILE* pf = fopen(path.toLocal8Bit(), "rb");
    if (pf == nullptr) {
        qWarning() << "RShxFont::load: Cannot open font file: " << path;
        return false;
    }
    char tmp[20];

    QString line;
    line = readLine(pf);
    bool ok = false;

    if (line.contains("unifont", Qt::CaseInsensitive)) {
        type = Unifont;
    }
    else if (line.contains("bigfont", Qt::CaseInsensitive)) {
        type = Bigfont;
        fread(tmp, 1, 3, pf);
        fread(&indexCount, 1, 2, pf);
        fread(&rangeCount, 1, 2, pf);
        for (int i = 0; i < rangeCount; i++) {
            EscapeRange range;
            fread(&range.start, 1, 2, pf);
            fread(&range.end, 1, 2, pf);
            ranges.push_back(range);
        }
        std::list<BigFontTable> tables;
        for (int i = 0; i < indexCount; i++)
        {
            fread(tmp, 1, 8, pf);
            BigFontTable table;
            table.ch = *(uint16_t*)tmp;
            table.offset = *(uint32_t*)(tmp + 4);
            table.len = *(uint16_t*)(tmp + 2);
            if(table.len > 0)
                tables.push_back(table);
        }

        char* indexTmp = nullptr;
        for (auto it = tables.begin(); it != tables.end(); it++) {
            fseek(pf, it->offset, SEEK_SET);
            indexTmp = (char*)realloc(indexTmp, it->len + 1);
            fread(indexTmp, 1, it->len, pf);
            if (it->ch == 0) {
                indexTmp[it->len] = 0;
                desc = indexTmp;
            }
            else
                parseDef(it->ch, indexTmp, it->len);
        }
        free(indexTmp);
        ok = true;
    }
    else if (line.contains("shapes", Qt::CaseInsensitive)) {
        type = Shapes;
        fread(tmp, 1, 5, pf);
        fread(&indexCount, 1, 2, pf);
        if (indexCount > 0) {
            indexes.resize(indexCount * 2);
            fread(&indexes[0], 1, indexCount * 4, pf);
            if (indexes[0] == 0) {
                type = Regfont;
                uint16_t firstShapeDef = indexes[1];

                char* indexTmp = nullptr;

                if (firstShapeDef > 0) {
                    indexTmp = (char*)realloc(indexTmp, firstShapeDef + 1);
                    fread(indexTmp, 1, firstShapeDef, pf);
                    indexTmp[firstShapeDef] = 0;
                    desc = indexTmp;
                }
                for (int i = 1; i < indexCount; i++) {
                    uint16_t ch = indexes[i * 2];
                    uint16_t len = indexes[i * 2 + 1];
                    if (len > 0) {
                        indexTmp = (char*)realloc(indexTmp, len);
                        fread(indexTmp, 1, len, pf);
                        parseDef(ch, indexTmp, len);
                    }
                }
                free(indexTmp);
                ok = true;
            }
        }
    }
    else {
        qWarning() << "RShxFont::load: invalid font file";
        ok = false;
    }

	return ok;
}

bool RShxFont::isEscapeChar(QChar ch) {
    QTextCodec* codec = QTextCodec::codecForName("GBK");
    if (codec == NULL)
        return false;
    auto bytes = codec->fromUnicode(ch);
    uint16_t gbk = (0xff & bytes.at(0));
    for (std::vector<EscapeRange>::size_type i = 0; i < ranges.size(); ++i)
    {
        if (gbk >= ranges[i].start && gbk <= ranges[i].end)
            return true;
    }
    return false;
}

QString RShxFont::readLine(FILE* pf)
{
    QString str;
    quint8 ch;
    while (true)
    {
        fread(&ch, 1, 1, pf);
        if (ch == 0x0a)
            break;
        if(ch != 0x0d)
            str.append(ch);
    }
    return str;
}

void RShxFont::parseDef(uint16_t ch, char* buf, int len)
{
    QString str;
    if (ch > 255)
    {
        QTextCodec* codec = QTextCodec::codecForName("GBK");
        if (codec == NULL)
            return;
        ch = qToBigEndian(ch);
        str = codec->toUnicode((char*)&ch, 2);
    }
    else
        str = (char)ch;
    if (str.length() != 1)
        return;

    buf++;
    len--;
    RPainterPath glyph;
    containsArcs = false;
    penX = 0;
    penY = 0; 
    scale = 1.;
    lines.clear();
    curLine.pts.clear();
    while (len > 0)
    {
        parseOneCode(buf, len);
    }
    endLine();

    for (auto it = lines.begin(); it != lines.end(); it++)
    {
        if (it->pts.size() > 1)
        {
            auto it2 = it->pts.begin();
            glyph.moveTo(it2->x, it2->y);
            it2++;
            for (; it2 != it->pts.end(); it2++) {
                glyph.lineTo(it2->x, it2->y);
            }
        }
    }
    lines.clear();
#ifdef _DEBUG
    glyph.setString(str);
#endif
    glyphMap.insert(str.at(0), glyph);
    if (containsArcs) {
        glyphDraftMap.insert(str.at(0), glyph);
    }
}

void RShxFont::parseOneCode(char* &buf, int &len)
{
    uint8_t code = *buf;
    ++buf;
    --len;
    switch (code)
    {
    case 0:
        return;
    case 1:
        drawMode = true;
        break;
    case 2:
        drawMode = false;
        break;
    case 3:
    {
        auto num = *buf;
        buf++;
        len--;
        scale /= num;
    }
    break;
    case 4:
    {
        auto num = *buf;
        buf++;
        len--;
        scale *= num;
    }
    break;
    case 5:
        penPosition.push(penX);
        penPosition.push(penY);
        break;
    case 6:
    {
        int sz = penPosition.size();
        if (sz > 1) {
            penX = penPosition.top();
            penPosition.pop();
            penY = penPosition.top();
            penPosition.pop();

            startLine();
        }
    }
    break;
    case 7:
    {
        uint16_t character;
        if (type == Unifont) {
            character = *(uint16_t*)buf;
            buf += 2;
            len -= 2;
        }
        else {
            character = *buf;
            buf ++;
            len --;
        }
    }
    break;
    case 8:
        caseCode8(buf, len);
        break;
    case 9:
        while (caseCode8(buf, len));
        break;
    case 0x0A:
    {
        int radius = *buf;
        ++buf;
        --len;
        char SC = *(char*)buf;
        ++buf;
        --len;
        int sign = 1;
        if (SC < 0)
        {
            sign = -1;
        }
        int    S = (SC & 0x70) >> 4;
        int    C = (SC & 0x07) * sign;
        double r = radius * scale;
        double SA = S * M_PI_4;
        double EA = (S + C) * M_PI_4;
        double CenterX = penX - r * cos(SA);
        double CenterY = penY - r * sin(SA);
        penX = CenterX + r * cos(EA);
        penY = CenterY + r * sin(EA);
        {
            if (C == 0)
                C = 8;
            drawArc(CenterX, CenterY, r, S * 45.0 / 180.0 * M_PI, C * 45.0 / 180.0 * M_PI);
        }
    }
        break;
    case 0x0B:
    {
        int start_offset = *buf;
        ++buf;
        --len;
        int end_offset = *buf;
        ++buf;
        --len;
        int high_radius = *buf;
        ++buf;
        --len;
        int radius = (*buf) + 256 * high_radius;
        ++buf;
        --len;
        char SC = *(char*)buf;
        ++buf;
        --len;
        int sign = 1;
        if (SC < 0)
        {
            sign = -1;
        }
        double S = ((SC & 0x70) >> 4) + sign * start_offset / 256.0;
        double C = (SC & 0x07);
        if (C == 0)
            C = 8;
        if (end_offset != 0)
            C = C - 1;
        C = (C + (end_offset - start_offset) / 256.0) * sign;
        double r = radius * scale;
        double SA = S * M_PI_4;
        double EA = (S + C) * M_PI_4;
        double CenterX = penX - r * cos(SA);
        double CenterY = penY - r * sin(SA);
        penX = CenterX + r * cos(EA);
        penY = CenterY + r * sin(EA);
        {
            drawArc(CenterX, CenterY, r, S * 45.0 / 180.0 * M_PI, C * 45.0 / 180.0 * M_PI);
        }
    }
        break;
    case 0x0C:
        caseCodeC(buf, len);
        break;
    case 0x0D:
        while (caseCodeC(buf, len));
        break;
    case 0x0E: 
    {
        double X = penX;
        double Y = penY;
        bool   bDrawMode = drawMode;
        parseOneCode( buf, len);
        drawMode = bDrawMode;
        penX = X;
        penY = Y;
    }
        break;
    case 0x0F:
        break;
    default:
        parseLenDirByte(code);
        break;
    }
}

void RShxFont::parseLenDirByte(uint8_t thebyte)
{
    int    len = (thebyte & 0xF0) >> 4;
    int    dir = thebyte & 0xF;
    double x = 0, y = 0;
    switch (dir)
    {
    case 0:
        x = 1;
        y = 0;
        break;
    case 1:
        x = 1;
        y = 0.5;
        break;
    case 2:
        x = 1;
        y = 1;
        break;
    case 3:
        x = 0.5;
        y = 1;
        break;
    case 4:
        x = 0;
        y = 1;
        break;
    case 5:
        x = -0.5;
        y = 1;
        break;
    case 6:
        x = -1;
        y = 1;
        break;
    case 7:
        x = -1;
        y = 0.5;
        break;
    case 8:
        x = -1;
        y = 0;
        break;
    case 9:
        x = -1;
        y = -0.5;
        break;
    case 10:
        x = -1;
        y = -1;
        break;
    case 11:
        x = -0.5;
        y = -1;
        break;
    case 12:
        x = 0;
        y = -1;
        break;
    case 13:
        x = 0.5;
        y = -1;
        break;
    case 14:
        x = 1;
        y = -1;
        break;
    case 15:
        x = 1;
        y = -0.5;
        break;
    default:
        break;
    }
    penX += x * len * scale;
    penY += y * len * scale;
    drawLine();
}

bool RShxFont::caseCode8(char*& buf, int& len)
{
    char* pChar = (char*)buf;
    int   dx = (*pChar);
    penX += scale * dx;
    ++buf;
    --len;
    pChar = (char*)buf;
    int dy = (*pChar);
    penY += scale * dy;
    ++buf;
    --len;
    if ((dx != 0) || (dy != 0))
    {
        drawLine();
        return true;
    }
    return false;
}

bool RShxFont::caseCodeC(char*& buf, int& len)
{
    char* pChar = (char*)buf;
    double X = penX;
    int    dx_noscale = (*pChar);
    ++buf;
    --len;
    pChar = (char*)buf;
    double Y = penY;
    int    dy_noscale = (*pChar);
    ++buf;
    --len;

    if ((dx_noscale != 0) || (dy_noscale != 0))
    {
        double dx = scale * dx_noscale;
        double dy = scale * dy_noscale;

        penX += dx;
        penY += dy;
        double dist = sqrt(dx * dx + dy * dy);
        {
            int    bulge = *(char*)buf;
            double ChordHeight = bulge * (dist / 2.0) / 127.0;
            double radius;
            double center_x, center_y;
            bool   bResult = calCRFromSEH(X, Y, penX, penY, ChordHeight, center_x, center_y, radius);

            if (bResult)
            {
                float startAngle = atan2f((float)(Y - center_y), (float)(X - center_x));
                float sweepAngle = 2.0f * (float)asin((dist / 2.0) / radius);
                if (bulge < 0)
                    sweepAngle = -sweepAngle;
                {
                    drawArc(center_x, center_y, radius, startAngle, sweepAngle);
                }
            }
            else
            {
                drawLine();
            }
        }

        ++buf;
        --len;
        return true;
    }
    return false;
}

bool RShxFont::calCRFromSEH(double startX, double startY, double endX, double endY, double chordHeight, double& centerX, double& centerY, double& radius)
{
    if (chordHeight == 0.0)
        return false;
    double dx = endX - startX;
    double dy = endY - startY;
    if (fabs(dx) <= 1E-3 && fabs(dy) == 1E-3)
        return false;
    double len = sqrt(dx * dx + dy * dy);
    double fabsH = fabs(chordHeight);
    radius = fabsH / 2 + len * len / (8.0 * fabsH);
    double CCDist;
    if (chordHeight > 0)
    {
        CCDist = chordHeight - radius;
    }
    else
    {
        CCDist = chordHeight + radius;
    }
    double pp = CCDist / len;
    centerX = (startX + endX) / 2.0 + dy * pp;
    centerY = (startY + endY) / 2.0 - dx * pp;
    return true;
}

void RShxFont::drawArc(double CenterX, double CenterY, double radius, double startAng, double sweepAng)
{
    int    segCount = (int)ceil(fabs(sweepAng) * 12.0 / M_PI);
    double detAng = sweepAng / segCount;
    double curAng = startAng;
    double pointX, pointY;
    pointX = CenterX + radius * cos(curAng);
    pointY = CenterY + radius * sin(curAng);
    startLine();
    for (int i = 1; i <= segCount; ++i)
    {
        curAng += detAng;
        pointX = CenterX + radius * cos(curAng);
        pointY = CenterY + radius * sin(curAng);
        
        ShxPoint pt;
        pt.x = pointX;
        pt.y = pointY;
        curLine.pts.emplace_back(pt);
    }
}

void RShxFont::drawLine()
{
    if (drawMode)
    {
        ShxPoint pt;
        pt.x = penX;
        pt.y = penY;
        curLine.pts.emplace_back(pt);
    }
    else
    {
        startLine();
    }
}

void RShxFont::endLine()
{
    if (curLine.pts.size() > 1)
    {
        lines.push_back(curLine);
    }
    curLine.pts.clear();
}

void RShxFont::startLine()
{
    endLine();
    ShxPoint pt;
    pt.x = penX;
    pt.y = penY;
    curLine.pts.emplace_back(pt);
}