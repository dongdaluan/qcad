#ifndef RSHXFONT_H
#define RSHXFONT_H

#include "RFont.h"
#include <stack>

class QCADCORE_EXPORT RShxFont : public RFont
{
public:
	enum Type {
		Unifont,
		Bigfont,
		Shapes,
		Regfont,
	};
	struct EscapeRange
	{
		uint16_t start;
		uint16_t end;
	};
	struct ShxPoint {
		double x;
		double y;
	};
	struct ShxLine
	{
		std::vector<ShxPoint> pts;
	};

	RShxFont(const QString& fileName);

	virtual bool load() override;
	bool isEscapeChar(QChar ch);
protected:
	QString readLine(FILE* pf);
	void parseDef(uint16_t ch, char* buf, int len);
	void parseOneCode(char* &buf, int& len);
	void parseLenDirByte(uint8_t code);
	bool caseCode8(char*& buf, int& len);
	bool caseCodeC(char*& buf, int& len);

	bool calCRFromSEH(double startX, double startY, double endX, double endY, double chordHeight, double& centerX, double& centerY, double& radius);
	void drawArc(double CenterX, double CenterY, double radius, double startAng, double sweepAng);
	void drawLine();
	void startLine();
	void endLine();

	double scale;
	bool drawMode;
	QString desc;
	Type type;
	uint16_t indexCount;
	uint16_t rangeCount;
	std::vector<EscapeRange> ranges;
	std::vector<uint16_t> indexes;

	std::vector<ShxLine> lines;
	ShxLine curLine;

	double penX;
	double penY;
	std::stack<double>       penPosition;
	bool containsArcs;
};


#endif