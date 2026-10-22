// SPDX-License-Identifier: GPL-3.0-or-later
// Рисует иконку приложения TwoonemoonSoundLab (1024×1024 PNG) средствами CoreGraphics.
// Собственная графика проекта: графитовая плашка, «нити» фона, стальная ручка с акцентной дугой.
// Запуск: swift Scripts/tools/make_icon.swift <out.png>
import CoreGraphics
import Foundation
import ImageIO
import UniformTypeIdentifiers

let size = 1024
let out = CommandLine.arguments.count > 1 ? CommandLine.arguments[1] : "icon_1024.png"
let cs = CGColorSpace(name: CGColorSpace.sRGB)!
let ctx = CGContext(data: nil, width: size, height: size, bitsPerComponent: 8, bytesPerRow: 0, space: cs,
                    bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)!
func rgb(_ r: CGFloat, _ g: CGFloat, _ b: CGFloat, _ a: CGFloat = 1) -> CGColor { CGColor(colorSpace: cs, components: [r / 255, g / 255, b / 255, a])! }
let S = CGFloat(size)

// Плашка macOS: скруглённый квадрат с полями (сетка Big Sur: 824/1024)
let inset: CGFloat = 100
let plate = CGRect(x: inset, y: inset, width: S - 2 * inset, height: S - 2 * inset)
let platePath = CGPath(roundedRect: plate, cornerWidth: 185, cornerHeight: 185, transform: nil)
// Тень
ctx.saveGState()
ctx.setShadow(offset: CGSize(width: 0, height: -14), blur: 36, color: rgb(0, 0, 0, 0.55))
ctx.addPath(platePath); ctx.setFillColor(rgb(17, 18, 22)); ctx.fillPath()
ctx.restoreGState()
// Графитовый градиент
ctx.saveGState()
ctx.addPath(platePath); ctx.clip()
let bg = CGGradient(colorsSpace: cs, colors: [rgb(38, 40, 47), rgb(12, 13, 16)] as CFArray, locations: [0, 1])!
ctx.drawLinearGradient(bg, start: CGPoint(x: S / 2, y: plate.maxY), end: CGPoint(x: S / 2, y: plate.minY), options: [])
// Вертикальные «архитектурные» нити
var seed: UInt64 = 7
func rnd() -> CGFloat { seed = seed &* 6364136223846793005 &+ 1442695040888963407; return CGFloat((seed >> 33) % 10000) / 10000 }
for i in 0..<46 {
  let x = plate.minX + CGFloat(i) / 46 * plate.width + rnd() * 8
  let top = plate.minY + rnd() * plate.height * 0.5
  ctx.setStrokeColor(rgb(255, 255, 255, 0.025 + rnd() * 0.035))
  ctx.setLineWidth(1.2 + rnd() * 1.5)
  ctx.move(to: CGPoint(x: x, y: top)); ctx.addLine(to: CGPoint(x: x, y: plate.maxY)); ctx.strokePath()
}
// Верхний блик стекла
let gloss = CGGradient(colorsSpace: cs, colors: [rgb(255, 255, 255, 0.10), rgb(255, 255, 255, 0)] as CFArray, locations: [0, 1])!
ctx.drawLinearGradient(gloss, start: CGPoint(x: S / 2, y: plate.maxY), end: CGPoint(x: S / 2, y: plate.maxY - 260), options: [])
ctx.restoreGState()
// Кромка
ctx.addPath(platePath); ctx.setStrokeColor(rgb(255, 255, 255, 0.10)); ctx.setLineWidth(3); ctx.strokePath()

// Ручка
let c = CGPoint(x: S / 2, y: S / 2 - 12)
let rArc: CGFloat = 268, rKnob: CGFloat = 205
let start = CGFloat.pi * 1.25, end = CGFloat.pi * -0.25 // дуга 270°, по часовой
let value: CGFloat = 0.68
// Дорожка дуги
ctx.setLineCap(.round)
ctx.setLineWidth(30)
ctx.setStrokeColor(rgb(255, 255, 255, 0.08))
ctx.addArc(center: c, radius: rArc, startAngle: start, endAngle: end, clockwise: true); ctx.strokePath()
// Акцентная дуга с градиентом magenta → violet
let vEnd = start - (start - end) * value
ctx.saveGState()
ctx.addArc(center: c, radius: rArc, startAngle: start, endAngle: vEnd, clockwise: true)
ctx.replacePathWithStrokedPath(); ctx.clip()
let acc = CGGradient(colorsSpace: cs, colors: [rgb(132, 102, 204), rgb(214, 104, 178)] as CFArray, locations: [0, 1])!
ctx.drawLinearGradient(acc, start: CGPoint(x: c.x - rArc, y: c.y - rArc), end: CGPoint(x: c.x + rArc, y: c.y + rArc), options: [])
ctx.restoreGState()
// Свечение конца дуги
let tip = CGPoint(x: c.x + rArc * cos(vEnd), y: c.y + rArc * sin(vEnd))
let glow = CGGradient(colorsSpace: cs, colors: [rgb(230, 120, 200, 0.75), rgb(230, 120, 200, 0)] as CFArray, locations: [0, 1])!
ctx.drawRadialGradient(glow, startCenter: tip, startRadius: 0, endCenter: tip, endRadius: 60, options: [])
// Тень ручки
ctx.saveGState()
ctx.setShadow(offset: CGSize(width: 0, height: -18), blur: 40, color: rgb(0, 0, 0, 0.7))
ctx.addEllipse(in: CGRect(x: c.x - rKnob, y: c.y - rKnob, width: 2 * rKnob, height: 2 * rKnob))
ctx.setFillColor(rgb(40, 42, 48)); ctx.fillPath()
ctx.restoreGState()
// Накатанный обод
for i in 0..<96 {
  let a = CGFloat(i) / 96 * 2 * .pi
  ctx.setStrokeColor(i % 2 == 0 ? rgb(90, 95, 104) : rgb(30, 32, 37))
  ctx.setLineWidth(5)
  ctx.move(to: CGPoint(x: c.x + (rKnob - 22) * cos(a), y: c.y + (rKnob - 22) * sin(a)))
  ctx.addLine(to: CGPoint(x: c.x + rKnob * cos(a), y: c.y + rKnob * sin(a))); ctx.strokePath()
}
// Шлифованная шапка (spun aluminium): радиальный градиент + концентрические штрихи
let rCap = rKnob - 30
ctx.saveGState()
ctx.addEllipse(in: CGRect(x: c.x - rCap, y: c.y - rCap, width: 2 * rCap, height: 2 * rCap)); ctx.clip()
let cap = CGGradient(colorsSpace: cs, colors: [rgb(232, 235, 240), rgb(158, 164, 174), rgb(96, 101, 110)] as CFArray, locations: [0, 0.55, 1])!
ctx.drawLinearGradient(cap, start: CGPoint(x: c.x - rCap * 0.6, y: c.y + rCap), end: CGPoint(x: c.x + rCap * 0.6, y: c.y - rCap), options: [])
for k in stride(from: CGFloat(6), to: rCap, by: 3.2) {
  ctx.setStrokeColor(rgb(255, 255, 255, 0.05 + 0.05 * rnd()))
  ctx.setLineWidth(1)
  ctx.addEllipse(in: CGRect(x: c.x - k, y: c.y - k, width: 2 * k, height: 2 * k)); ctx.strokePath()
}
// Блик-«луч» шлифовки
let ray = CGGradient(colorsSpace: cs, colors: [rgb(255, 255, 255, 0), rgb(255, 255, 255, 0.35), rgb(255, 255, 255, 0)] as CFArray, locations: [0, 0.5, 1])!
ctx.drawLinearGradient(ray, start: CGPoint(x: c.x - rCap, y: c.y - rCap * 0.2), end: CGPoint(x: c.x + rCap, y: c.y + rCap * 0.2), options: [])
ctx.restoreGState()
ctx.addEllipse(in: CGRect(x: c.x - rCap, y: c.y - rCap, width: 2 * rCap, height: 2 * rCap))
ctx.setStrokeColor(rgb(255, 255, 255, 0.35)); ctx.setLineWidth(2.5); ctx.strokePath()
// Указатель
ctx.setLineCap(.round)
ctx.setStrokeColor(rgb(24, 24, 28)); ctx.setLineWidth(18)
ctx.move(to: CGPoint(x: c.x + 40 * cos(vEnd), y: c.y + 40 * sin(vEnd)))
ctx.addLine(to: CGPoint(x: c.x + (rCap - 26) * cos(vEnd), y: c.y + (rCap - 26) * sin(vEnd))); ctx.strokePath()

let img = ctx.makeImage()!
let dest = CGImageDestinationCreateWithURL(URL(fileURLWithPath: out) as CFURL, UTType.png.identifier as CFString, 1, nil)!
CGImageDestinationAddImage(dest, img, nil)
CGImageDestinationFinalize(dest)
print(out)
