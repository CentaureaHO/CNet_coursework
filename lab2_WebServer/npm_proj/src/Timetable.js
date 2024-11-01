import * as d3 from 'd3';
import * as XLSX from 'xlsx';

document.getElementById('InputData').addEventListener('change', FileHandler, false);

function FileHandler(Event) 
{
  d3.select("#TimetableContainer").selectAll("svg").remove();
  const File = Event.target.files[0];
  const Reader = new FileReader();
  Reader.onload = function(e) 
  {
    const Data = e.target.result;
    const WorkBook = ParseExcelFile(Data);
    const { DailyData, CategoryToColor, ScaleFactor } = ProcessData(WorkBook);
    ChartSetup(DailyData, CategoryToColor, ScaleFactor);
    UiSetup(ScaleFactor, CategoryToColor);
  };
  Reader.readAsBinaryString(File);
}

function ParseExcelFile(Data) 
{
  return XLSX.read(Data, {type: 'binary'});
}

function ProcessData(Workbook) 
{
  const WorkSheet = Workbook.Sheets[Workbook.SheetNames[0]];
  const Ref = XLSX.utils.decode_range(WorkSheet['!ref']);
  const Rows = Ref.e.r;
  const Cols = Ref.e.c;
  let ScaleFactor = 0;
  let ColorColumnIndex = null;
  const CategoryToColor = {};
  const DailyData = {};
  for (let C = Ref.s.c; C <= Cols; ++C) 
  {
    const CellRef = XLSX.utils.encode_cell({ c: C, r: Ref.s.r });
    const Cell = WorkSheet[CellRef];
    if (Cell && Cell.v === "颜色") ColorColumnIndex = C;
  }
  for (let C = Ref.s.c + 1; C <= Cols; ++C) 
  {
    if (C === ColorColumnIndex) continue;
    let ColSum = 0;
    for (let R = Ref.s.r + 1; R <= Rows; ++R) 
    {
      const Cell = WorkSheet[XLSX.utils.encode_cell({ c: C, r: R })];
      if (Cell && !isNaN(Cell.v)) ColSum += Cell.v;
    }
    if (ColSum > ScaleFactor) ScaleFactor = ColSum;
  }
  ScaleFactor *= 1.1;
  for (let R = Ref.s.r + 1; R <= Rows; ++R) 
  {
    const CategoryCellRef = XLSX.utils.encode_cell({ c: Ref.s.c, r: R });
    const Category = WorkSheet[CategoryCellRef] ? WorkSheet[CategoryCellRef].v : null;
    if (Category) 
    {
      if (ColorColumnIndex !== null) 
      {
        const ColorCellRef = XLSX.utils.encode_cell({ c: ColorColumnIndex, r: R });
        const ColorCell = WorkSheet[ColorCellRef];
        CategoryToColor[Category] = ColorCell ? ColorCell.v : null;
      }
      if (!CategoryToColor[Category]) 
      {
        CategoryToColor[Category] = `#${Math.floor(Math.random()*16777215).toString(16)}`;
      }
    }
  }
  for (let C = Ref.s.c + 1; C <= Cols; ++C) 
  {
    if (C === ColorColumnIndex) continue;
    const DateCellRef = XLSX.utils.encode_cell({ c: C, r: Ref.s.r });
    const DateCellValue = WorkSheet[DateCellRef] ? WorkSheet[DateCellRef].v : `Column ${C + 1}`;
    DailyData[DateCellValue] = [];
    for (let R = Ref.s.r + 1; R <= Rows; ++R) 
    {
      const TimeCellRef = XLSX.utils.encode_cell({ c: C, r: R });
      const TimeCell = WorkSheet[TimeCellRef];
      const CategoryCellRef = XLSX.utils.encode_cell({ c: Ref.s.c, r: R });
      const Category = WorkSheet[CategoryCellRef] ? WorkSheet[CategoryCellRef].v : null;
      if (TimeCell && Category) 
      {
        DailyData[DateCellValue].push(
        {
          category: Category,
          value: TimeCell.v,
          color: CategoryToColor[Category]
        });
      }
    }
    DailyData[DateCellValue].sort((a, b) => a.value - b.value);
  }
  return { DailyData, CategoryToColor, ScaleFactor };
}

function ChartSetup(DailyData, CategoryToColor, ScaleFactor)
{
  console.log(DailyData)
  const Width = window.innerWidth * 0.9;
  const Height = window.innerHeight * 0.8;
  const Margin = { top: 20, right: 20, bottom: 30, left: 50 };
  const Svg = d3.select('#TimetableContainer')
                .append('svg')
                .attr('width', Width + Margin.left + Margin.right)
                .attr('height', Height + Margin.top + Margin.bottom)
                .style('background-color', 'white')
                .append('g')
                .attr('transform', `translate(${Margin.left}, ${Margin.top})`);
  const XScale = d3.scaleBand()
                   .range([0, Width])
                   .paddingInner(0.3)
                   .paddingOuter(0.05)
                   .domain(Object.keys(DailyData));
  Svg.append('g')
     .attr('transform', `translate(0, ${Height})`)
     .call(d3.axisBottom(XScale))
     .selectAll("text")
     .attr('display', 'none');
  Object.keys(DailyData).forEach((Date, Index) => 
  {
    Svg.append('text')
       .attr('x', XScale(Date) + XScale.bandwidth() / 2)
       .attr('y', Height + Margin.bottom / 2)
       .attr('text-anchor', 'middle')
       .text(Date);
  });
  Object.keys(DailyData).forEach((Date, Index) => 
  {
    let CurHeight = 0;
    DailyData[Date].forEach(Item => 
    {
      const BarHeight = (Item.value / ScaleFactor) * (Height - Margin.top - Margin.bottom);
      Svg.append('rect')
         .attr('x', XScale(Date))
         .attr('y', Height - CurHeight - BarHeight)
         .attr('width', XScale.bandwidth())
         .attr('height', BarHeight)
         .attr('fill', Item.color);
      Svg.append('line')
         .attr('x1', XScale(Date))
         .attr('y1', Height - CurHeight - BarHeight)
         .attr('x2', XScale(Date) + XScale.bandwidth())
         .attr('y2', Height - CurHeight - BarHeight)
         .attr('stroke', 'white')
         .attr('stroke-width', 2);
      Svg.append('line')
         .attr('x1', XScale(Date))
         .attr('y1', Height - CurHeight)
         .attr('x2', XScale(Date) + XScale.bandwidth())
         .attr('y2', Height - CurHeight)
         .attr('stroke', 'white')
         .attr('stroke-width', 2);
      Svg.append('line')
         .attr('x1', XScale(Date))
         .attr('y1', Height - CurHeight - BarHeight)
         .attr('x2', XScale(Date))
         .attr('y2', Height - CurHeight)
         .attr('stroke', Item.color)
         .attr('stroke-width', 1);
      Svg.append('line')
         .attr('x1', XScale(Date) + XScale.bandwidth())
         .attr('y1', Height - CurHeight - BarHeight)
         .attr('x2', XScale(Date) + XScale.bandwidth())
         .attr('y2', Height - CurHeight)
         .attr('stroke', Item.color)
         .attr('stroke-width', 1);
      CurHeight += BarHeight;
    });
    if (Index === Object.keys(DailyData).length - 1) 
      Svg.append('line')
         .attr('x1', XScale(Date) + XScale.bandwidth())
         .attr('y1', Height - CurHeight)
         .attr('x2', XScale(Date) + XScale.bandwidth())
         .attr('y2', Height)
         .attr('stroke', 'white')
         .attr('stroke-width', 2);
    if (Index === 0)
      Svg.append('line')
         .attr('x1', XScale(Date))
         .attr('y1', Height - CurHeight)
         .attr('x2', XScale(Date))
         .attr('y2', Height)
         .attr('stroke', 'white')
         .attr('stroke-width', 2);
  });
  const DateIndices = Object.keys(DailyData).reduce((Acc, Date, Index) => 
  {
    Acc[Date] = Index;
    return Acc;
  }, {});
  Object.keys(DailyData).forEach((Date, Index) => 
  {
    if (Index < Object.keys(DailyData).length - 1) 
    {
      const NextDate = Object.keys(DailyData)[Index + 1];
      let CurHeight = 0;
      DailyData[Date].forEach(Item => 
      {
        const BarHeight = (Item.value / ScaleFactor) * (Height - Margin.top - Margin.bottom);
        const YTop = Height - CurHeight - BarHeight;
        const YBottom = YTop + BarHeight;
        const X1 = XScale(Date) + XScale.bandwidth();
        const X2 = XScale(NextDate);
        const NextBarIndex = DailyData[NextDate].findIndex(nextItem => nextItem.category === Item.category);
        if (NextBarIndex !== -1)
        {
          let NextCurHeigh = 0;
          for (let j = 0; j < NextBarIndex; j++) NextCurHeigh += (DailyData[NextDate][j].value / ScaleFactor) * (Height - Margin.top - Margin.bottom);
          const NextBarHeight = (DailyData[NextDate][NextBarIndex].value / ScaleFactor) * (Height - Margin.top - Margin.bottom);
          const NextYTop = Height - NextCurHeigh - NextBarHeight;
          const NextYBottom = NextYTop + NextBarHeight;
          ConnectBars(Svg, X1, YTop, YBottom, X2, NextYTop, NextYBottom, Item.color);
        }
        CurHeight += BarHeight;
      });
    }
  });
  Svg.append('line')
     .attr('x1', 0)
     .attr('y1', 0.1 * Height)
     .attr('x2', 0)
     .attr('y2', Height)
     .attr('stroke', 'black');
  Svg.append('line')
     .attr('x1', Width)
     .attr('y1', 0.1 * Height)
     .attr('x2', Width)
     .attr('y2', Height)
     .attr('stroke', 'black');
  //前边绘制边框导致遮挡的部分不想调整了，干脆再画一条，反正看着都一样
  Svg.append('g')
     .attr('transform', `translate(0, ${Height})`)
     .call(d3.axisBottom(XScale))
     .selectAll("text")
     .attr('display', 'none');
}  

function ConnectBars(Svg, X1, Y1Top, Y1Bottom, X2, Y2Top, Y2Bottom, Color) 
{
  const ControlX1 = (X2 + X1) / 2;
  const ControlX2 = (X2 + X1) / 2;
  Svg.append('path')
     .attr('d', `M${X1}, ${Y1Top}
                   C${ControlX1}, ${Y1Top} ${ControlX2}, ${Y2Top} ${X2}, ${Y2Top}
                   L${X2}, ${Y2Bottom}
                   C${ControlX2}, ${Y2Bottom} ${ControlX1}, ${Y1Bottom} ${X1}, ${Y1Bottom}
                   Z`)
     .attr('fill', Color);
  Svg.append('path')
     .attr('d', `M${X1}, ${Y1Top} C${ControlX1}, ${Y1Top} ${ControlX2}, ${Y2Top} ${X2}, ${Y2Top}`)
     .attr('stroke', 'white')
     .attr('stroke-width', 2)
     .attr('fill', 'none');
  Svg.append('path')
      .attr('d', `M${X1}, ${Y1Bottom} C${ControlX1}, ${Y1Bottom} ${ControlX2}, ${Y2Bottom} ${X2}, ${Y2Bottom}`)
      .attr('stroke', 'white')
      .attr('stroke-width', 2)
      .attr('fill', 'none');
}

function UiSetup(ScaleFactor, CategoryToColor)
{
  const ScaleFactorElement = document.getElementById('ScaleFactor');
  if (ScaleFactorElement) ScaleFactorElement.textContent = `Scale Factor: ${ScaleFactor / 1.1}`;
  const LegendContainer = document.getElementById('ColorMappingContainer');
  if (LegendContainer)
  {
    LegendContainer.innerHTML = '';
    Object.entries(CategoryToColor).forEach(([Category, Color]) => 
    {
      const ColorBlock = document.createElement('div');
      ColorBlock.style.backgroundColor = Color;
      ColorBlock.style.width = '20px';
      ColorBlock.style.height = '20px';
      ColorBlock.style.display = 'inline-block';
      ColorBlock.style.margin = '0 10px 10px 0';
      ColorBlock.style.border = '1px solid #ccc';
      const ColorLabel = document.createElement('span');
      ColorLabel.textContent = Category;
      const ColorItem = document.createElement('div');
      ColorItem.style.display = 'flex';
      ColorItem.style.alignItems = 'center';
      ColorItem.appendChild(ColorBlock);
      ColorItem.appendChild(ColorLabel);
      LegendContainer.appendChild(ColorItem);
    });
  }
}  

function KaiBai() 
{
  return{ Sheets: 
          {
            'Sheet1': 
            {
              'A1': { 'v': '分类' }, 'B1': { 'v': '周日' }, 'C1': { 'v': '周一' }, 'D1': { 'v': '周二' }, 'E1': { 'v': '周三' }, 'F1': { 'v': '周四' }, 'G1': { 'v': '周五' }, 'H1': { 'v': '周六' }, 'I1': { 'v': '颜色' },
              'A2': { 'v': '睡觉' }, 'B2': { 'v': 562 }, 'C2': { 'v': 450 }, 'D2': { 'v': 439 }, 'E2': { 'v': 523 }, 'F2': { 'v': 486 }, 'G2': { 'v': 517 }, 'H2': { 'v': 520 }, 'I2': { 'v': '#540D6E' },
              'A3': { 'v': '学习' }, 'B3': { 'v': 327 }, 'C3': { 'v': 583 }, 'D3': { 'v': 636 }, 'E3': { 'v': 367 }, 'F3': { 'v': 500 }, 'G3': { 'v': 309 }, 'H3': { 'v': 393 }, 'I3': { 'v': '#F24333' },
              'A4': { 'v': '娱乐' }, 'B4': { 'v': 355 }, 'C4': { 'v': 202 }, 'D4': { 'v': 144 }, 'E4': { 'v': 248 }, 'F4': { 'v': 269 }, 'G4': { 'v': 155 }, 'H4': { 'v': 346 }, 'I4': { 'v': '#3BCEAC' },
              'A5': { 'v': '锻炼' }, 'B5': { 'v': 83  }, 'C5': { 'v': 53  }, 'D5': { 'v': 0   }, 'E5': { 'v': 156 }, 'F5': { 'v': 59  }, 'G5': { 'v': 49  }, 'H5': { 'v': 83  }, 'I5': { 'v': '#0EAD69' },
              'A6': { 'v': '进食' }, 'B6': { 'v': 68  }, 'C6': { 'v': 52  }, 'D6': { 'v': 69  }, 'E6': { 'v': 59  }, 'F6': { 'v': 67  }, 'G6': { 'v': 63  }, 'H6': { 'v': 68  }, 'I6': { 'v': '#FFD23F' },
              'A7': { 'v': '其它' }, 'B7': { 'v': 45  }, 'C7': { 'v': 100  }, 'D7': { 'v': 152  }, 'E7': { 'v': 87  }, 'F7': { 'v': 59  }, 'G7': { 'v': 347  }, 'H7': { 'v': 30  }, 'I7': { 'v': '#72B4B9' },
              '!ref': 'A1:I7',
            }
          },
          SheetNames: ['Sheet1']
        };
}

document.addEventListener('DOMContentLoaded', (event) => 
{
  const InitWorkBook = KaiBai();
  let { DailyData, CategoryToColor, ScaleFactor } = ProcessData(InitWorkBook);
  ChartSetup(DailyData, CategoryToColor, ScaleFactor);
  UiSetup(ScaleFactor, CategoryToColor);
});