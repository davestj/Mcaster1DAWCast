// generate-app-icon.cpp — Generates a professional 3D app icon for Mcaster1DAWCast
// Compile: g++ -std=c++17 $(pkg-config --cflags --libs Qt6Gui Qt6Widgets) -o generate-app-icon generate-app-icon.cpp
// Run:     ./generate-app-icon

#include <QApplication>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QLinearGradient>
#include <QConicalGradient>
#include <QDir>
#include <cmath>

static QImage renderIcon(int size)
{
    QImage img(size, size, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const double s = size / 512.0;  // scale factor
    const double margin = 16 * s;
    const double w = size - 2 * margin;
    const double cornerRadius = 96 * s;

    // ── Rounded square background with 3D depth ──────────────────────
    QPainterPath bgPath;
    bgPath.addRoundedRect(margin, margin, w, w, cornerRadius, cornerRadius);

    // Base: deep dark gradient
    QLinearGradient bgGrad(margin, margin, size - margin, size - margin);
    bgGrad.setColorAt(0.0, QColor(30, 40, 64));
    bgGrad.setColorAt(0.4, QColor(21, 24, 33));
    bgGrad.setColorAt(1.0, QColor(15, 17, 25));
    p.fillPath(bgPath, bgGrad);

    // Inner border — subtle metallic edge
    QPen borderPen(QColor(50, 55, 75), 2.0 * s);
    p.setPen(borderPen);
    p.drawPath(bgPath);

    // 3D highlight — top-left light source
    QLinearGradient hlGrad(margin, margin, size * 0.5, size * 0.5);
    hlGrad.setColorAt(0.0, QColor(255, 255, 255, 35));
    hlGrad.setColorAt(0.5, QColor(255, 255, 255, 8));
    hlGrad.setColorAt(1.0, QColor(255, 255, 255, 0));
    p.fillPath(bgPath, hlGrad);

    // Teal glow behind waveform
    QRadialGradient glow(size * 0.50, size * 0.40, size * 0.30);
    glow.setColorAt(0.0, QColor(45, 212, 191, 50));
    glow.setColorAt(0.5, QColor(45, 212, 191, 20));
    glow.setColorAt(1.0, QColor(45, 212, 191, 0));
    p.fillPath(bgPath, glow);

    // ── Circular meter ring (3D effect) ──────────────────────────────
    const double cx = size * 0.50;
    const double cy = size * 0.38;
    const double ringR = size * 0.22;

    // Outer ring shadow
    QRadialGradient ringShadow(cx, cy + 4 * s, ringR + 8 * s);
    ringShadow.setColorAt(0.85, QColor(0, 0, 0, 0));
    ringShadow.setColorAt(1.0, QColor(0, 0, 0, 60));
    p.setBrush(ringShadow);
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPointF(cx, cy + 4 * s), ringR + 8 * s, ringR + 8 * s);

    // Metallic ring — conical gradient for 3D chrome effect
    QConicalGradient ringGrad(cx, cy, 135);
    ringGrad.setColorAt(0.00, QColor(80, 85, 100));
    ringGrad.setColorAt(0.15, QColor(140, 145, 160));
    ringGrad.setColorAt(0.30, QColor(60, 65, 80));
    ringGrad.setColorAt(0.50, QColor(100, 105, 120));
    ringGrad.setColorAt(0.70, QColor(55, 60, 75));
    ringGrad.setColorAt(0.85, QColor(130, 135, 150));
    ringGrad.setColorAt(1.00, QColor(80, 85, 100));
    p.setBrush(ringGrad);
    p.setPen(QPen(QColor(40, 44, 58), 1.5 * s));
    p.drawEllipse(QPointF(cx, cy), ringR, ringR);

    // Inner disc — dark recessed area
    const double innerR = ringR - 10 * s;
    QRadialGradient innerGrad(cx - 8 * s, cy - 8 * s, innerR * 1.2);
    innerGrad.setColorAt(0.0, QColor(25, 28, 40));
    innerGrad.setColorAt(1.0, QColor(12, 14, 22));
    p.setBrush(innerGrad);
    p.setPen(QPen(QColor(35, 38, 52), 1 * s));
    p.drawEllipse(QPointF(cx, cy), innerR, innerR);

    // ── Audio waveform inside the ring ───────────────────────────────
    const int barCount = 15;
    const double barW = 4.0 * s;
    const double barSpacing = (innerR * 1.6) / barCount;
    const double barStartX = cx - (barCount * barSpacing) / 2.0 + barSpacing / 2.0;

    // Waveform amplitudes (symmetric, broadcast-style)
    const double amps[] = {0.15, 0.30, 0.55, 0.40, 0.70, 0.50, 0.85, 0.60, 0.90, 0.55, 0.75, 0.45, 0.60, 0.28, 0.12};

    for (int i = 0; i < barCount; ++i) {
        double x = barStartX + i * barSpacing;
        double h = amps[i] * innerR * 0.80;

        // Teal gradient per bar — brighter in the center
        QLinearGradient barGrad(x, cy - h, x, cy + h);
        barGrad.setColorAt(0.0, QColor(45, 212, 191, 200));
        barGrad.setColorAt(0.3, QColor(94, 235, 212, 255));
        barGrad.setColorAt(0.5, QColor(94, 235, 212, 255));
        barGrad.setColorAt(0.7, QColor(45, 212, 191, 200));
        barGrad.setColorAt(1.0, QColor(30, 180, 160, 150));

        p.setPen(Qt::NoPen);
        p.setBrush(barGrad);

        // Rounded bar caps
        QPainterPath barPath;
        barPath.addRoundedRect(x - barW / 2, cy - h, barW, h * 2, barW / 2, barW / 2);
        p.drawPath(barPath);

        // Glow per bar
        p.setBrush(QColor(45, 212, 191, 30));
        p.drawEllipse(QPointF(x, cy), barW * 1.5, h * 0.8);
    }

    // ── Broadcast indicator (top-right of ring) ──────────────────────
    const double bx = cx + ringR * 0.65;
    const double by = cy - ringR * 0.65;

    // Red dot with glow
    QRadialGradient redGlow(bx, by, 12 * s);
    redGlow.setColorAt(0.0, QColor(239, 68, 68, 200));
    redGlow.setColorAt(0.5, QColor(239, 68, 68, 60));
    redGlow.setColorAt(1.0, QColor(239, 68, 68, 0));
    p.setBrush(redGlow);
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPointF(bx, by), 12 * s, 12 * s);

    // Red dot core
    p.setBrush(QColor(239, 68, 68));
    p.drawEllipse(QPointF(bx, by), 5 * s, 5 * s);

    // Broadcast arcs
    p.setPen(QPen(QColor(239, 68, 68, 180), 2.0 * s, Qt::SolidLine, Qt::RoundCap));
    p.setBrush(Qt::NoBrush);
    p.drawArc(QRectF(bx - 10 * s, by - 10 * s, 20 * s, 20 * s), 30 * 16, 120 * 16);
    p.drawArc(QRectF(bx - 16 * s, by - 16 * s, 32 * s, 32 * s), 30 * 16, 120 * 16);

    // ── "MCASTER1" text ──────────────────────────────────────────────
    QFont brandFont("Helvetica Neue", 18 * s, QFont::Medium);
    brandFont.setLetterSpacing(QFont::AbsoluteSpacing, 3 * s);
    p.setFont(brandFont);
    p.setPen(QColor(136, 146, 164));
    p.drawText(QRectF(0, size * 0.64, size, 30 * s), Qt::AlignCenter, "MCASTER1");

    // ── "DAWCast" text — bold, prominent ─────────────────────────────
    QFont nameFont("Helvetica Neue", 44 * s, QFont::ExtraBold);
    nameFont.setLetterSpacing(QFont::AbsoluteSpacing, 2 * s);
    p.setFont(nameFont);

    // Text shadow
    p.setPen(QColor(0, 0, 0, 80));
    p.drawText(QRectF(2 * s, size * 0.70 + 2 * s, size, 60 * s), Qt::AlignCenter, "DAWCast");

    // Main text with gradient
    QLinearGradient textGrad(size * 0.3, size * 0.72, size * 0.7, size * 0.72);
    textGrad.setColorAt(0.0, QColor(240, 240, 240));
    textGrad.setColorAt(0.5, QColor(255, 255, 255));
    textGrad.setColorAt(1.0, QColor(220, 225, 235));
    p.setPen(QPen(QBrush(textGrad), 0));
    p.drawText(QRectF(0, size * 0.70, size, 60 * s), Qt::AlignCenter, "DAWCast");

    // ── Subtle bottom reflection ─────────────────────────────────────
    QLinearGradient reflGrad(0, size - margin - 40 * s, 0, size - margin);
    reflGrad.setColorAt(0.0, QColor(255, 255, 255, 0));
    reflGrad.setColorAt(1.0, QColor(255, 255, 255, 6));
    p.fillPath(bgPath, reflGrad);

    p.end();
    return img;
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    QString outDir = QDir::currentPath() + "/image_resources";
    QString iconsetDir = outDir + "/Mcaster1DAWCast.iconset";
    QDir().mkpath(iconsetDir);

    // Generate all sizes
    struct IconSize { int size; QString name; };
    QList<IconSize> sizes = {
        {16,   "icon_16x16.png"},
        {32,   "icon_16x16@2x.png"},
        {32,   "icon_32x32.png"},
        {64,   "icon_32x32@2x.png"},
        {128,  "icon_128x128.png"},
        {256,  "icon_128x128@2x.png"},
        {256,  "icon_256x256.png"},
        {512,  "icon_256x256@2x.png"},
        {512,  "icon_512x512.png"},
        {1024, "icon_512x512@2x.png"},
    };

    for (const auto& sz : sizes) {
        QImage img = renderIcon(sz.size);
        QString path = iconsetDir + "/" + sz.name;
        img.save(path, "PNG");
        printf("Generated %s (%dx%d)\n", qPrintable(sz.name), sz.size, sz.size);
    }

    // Also save a 1024px master PNG
    QImage master = renderIcon(1024);
    master.save(outDir + "/app-icon-1024.png", "PNG");
    printf("Generated app-icon-1024.png (1024x1024)\n");

    printf("\nDone. Run: iconutil -c icns %s -o %s/app-icon.icns\n",
           qPrintable(iconsetDir), qPrintable(outDir));
    return 0;
}
