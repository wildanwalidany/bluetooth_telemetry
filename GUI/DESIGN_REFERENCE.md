# Visual Design Reference

## Color Palette

### Primary Colors
```
Blue    : #3498db  (Controls, Speed normal)
Green   : #27ae60  (Success, Battery good, Active states)
Orange  : #f39c12  (Statistics, Server waiting)
Red     : #e74c3c  (Errors, Critical values)
Purple  : #9b59b6  (Status section)
Teal    : #16a085  (Telemetry section)
```

### Background Colors
```
White      : #ffffff  (Panels)
Light Gray : #f5f6fa  (Main background)
Dark Gray  : #2c3e50  (Log console)
Light Text : #ecf0f1  (Console text)
```

### Text Colors
```
Primary   : #2c3e50  (Main text)
Secondary : #34495e  (Labels)
Muted     : #7f8c8d  (Inactive)
Inactive  : #95a5a6  (Disabled)
```

## Component Specifications

### Buttons
```
Height        : 40px
Border Radius : 6px
Padding       : 10px 20px
Font Size     : 14px
Font Weight   : Bold
```

**Start Button**
- Background: #27ae60 (green)
- Hover: #2ecc71
- Pressed: #229954

**Stop Button**
- Background: #e74c3c (red)
- Hover: #ec7063
- Pressed: #c0392b

### GroupBoxes
```
Border        : 2px solid
Border Radius : 8px
Padding Top   : 15px
Margin Top    : 10px
Title Padding : 0 10px
Font Size     : 14px
Font Weight   : Bold
```

### Status Indicator
```
Size       : 32px font
Character  : ● (bullet)
Colors:
  - Gray   : #95a5a6 (stopped)
  - Orange : #f39c12 (waiting)
  - Green  : #2ecc71 (connected)
```

### State Indicators
```
Width         : 16px
Height        : 16px
Border Radius : 8px (circular)
Active Color  : #2ecc71 (green)
Inactive Color: #95a5a6 (gray)
```

### Progress Bars
```
Height        : 20px
Border        : 2px solid
Border Radius : 5px
Background    : #ecf0f1
Chunk Radius  : 3px
```

**Battery Progress**
- Good (>50%): Gradient #27ae60 → #2ecc71
- Medium (20-50%): Solid #f39c12
- Low (<20%): Solid #e74c3c

**Temperature Progress**
- Gradient: #3498db (cold) → #f39c12 (warm) → #e74c3c (hot)
- Range: -20°C to 43°C

### Speed Display
```
Font Size   : 24px
Font Weight : Bold
Padding     : 10px 20px
Border Radius: 6px

Normal (<5000):   Color #3498db, Background #ecf0f1
High (5000-8000): Color #f39c12, Background #fef5e7
Critical (>8000): Color #e74c3c, Background #fadbd8
```

### Log Console
```
Background    : #2c3e50 (dark)
Text Color    : #ecf0f1 (light)
Font Family   : 'Courier New', monospace
Font Size     : 12px
Border        : 1px solid #34495e
Border Radius : 4px
Padding       : 10px
Max Height    : 200px
```

### Labels
```
Primary Data:
  Font Size   : 14px
  Font Weight : Bold
  Color       : #34495e
  Padding     : 5px

Large Display (Speed):
  Font Size   : 24px
  Font Weight : Bold
  Padding     : 10px 20px

Statistics:
  Font Size   : 18px
  Font Weight : Bold
  Padding     : 8px 15px
  Background  : #ecf0f1
  Border Radius: 5px
  Min Width   : 80-100px
```

## Layout Specifications

### Main Layout
```
Spacing         : 15px
Margins         : 20px all sides
```

### GroupBox Content
```
Spacing         : 12px (telemetry), 15-20px (others)
Content Margins : 15px (horizontal), 15-20px (vertical)
```

### Grid Layout (Telemetry)
```
Columns      : 6 (split into two groups)
Row Spacing  : 12px
Left Group   : Columns 0-3 (metrics)
Right Group  : Columns 4-5 (states)
```

## Typography Scale

```
Extra Large : 32px (Status indicator)
Large       : 24px (Speed display)
Medium-Large: 18px (Statistics)
Medium      : 14px (Primary labels, data)
Small       : 13px (Secondary text, checkboxes)
Tiny        : 12px (Log console)
```

## State Color Mappings

### Drive State
- N (Neutral): Default
- D (Drive): Default
- P (Park): Default

### Drive Mode
- ECON: #27ae60 (green)
- COMF: #3498db (blue)
- SPORT: #e74c3c (red)

### Turn Signals
- None: #95a5a6 (gray)
- Right/Left: #2ecc71 (green)
- Hazard: #f39c12 (orange)

### Binary States (ON/OFF)
- Active: #2ecc71 (green indicator), #27ae60 (text)
- Inactive: #95a5a6 (indicator), #7f8c8d (text)

### Alert Level
- 0: #27ae60 (green, normal)
- >0: #e74c3c (red with background #fadbd8)

## Spacing Guidelines

```
Between Sections      : 15px
Within Section        : 12px
Button Spacing        : 15px
Indicator to Label    : 5px
Label to Data         : Natural flow
Panel Padding         : 15-20px
Title Offset          : 10px
```

## Best Practices

1. **Consistency**: Use defined colors consistently
2. **Contrast**: Ensure text is readable (WCAG AA minimum)
3. **States**: Always provide visual feedback for state changes
4. **Hierarchy**: Use size and weight to establish importance
5. **Spacing**: Don't overcrowd, use whitespace effectively
6. **Colors**: Limit palette to maintain cohesion
7. **Accessibility**: Consider colorblind users (use icons + text)
8. **Performance**: Minimize redraws, use efficient updates
