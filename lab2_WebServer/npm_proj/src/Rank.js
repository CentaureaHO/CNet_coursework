import * as d3 from 'd3';
import * as XLSX from 'xlsx';

document.getElementById('DataInput').addEventListener('change', function(event) { FileHandler(event.target.files[0]);} );

function FileHandler(File) 
{
    const Reader = new FileReader();
    Reader.onload = function(event) 
    {
        const Workbook = XLSX.read(new Uint8Array(event.target.result), {type: 'array'});
        const FirstSheetName = Workbook.SheetNames[0];
        const Sheet = Workbook.Sheets[FirstSheetName];
        console.log(Sheet);
        const DataJson = XLSX.utils.sheet_to_json(Sheet, { header: 1 });
        const {ACMap, AwardArray} = ProcessData(DataJson);
        const ArrayToDraw = EntitiesSorter(AwardArray);
        Temp(ACMap, ArrayToDraw, AwardArray);
    };
    Reader.readAsArrayBuffer(File);
}

function Temp(ACMap, ArrayToDraw, AwardArray)
{
    DrawChart(ACMap, ArrayToDraw);
    SetupButtons(ACMap, AwardArray);
}

function ProcessData(Data) 
{
    let ACMap = {};
    let AwardArray = [];
    const Headers = Data[0];
    let AwardNames = [];
    for (let i = 1; i < Headers.length; i++) 
    {
        const header = Headers[i];
        if (header.toLowerCase() === '颜色') ACMap[AwardNames[AwardNames.length - 1]] = Data[1][i] ? Data[1][i] : `#${Math.floor(Math.random() * 16777215).toString(16)}`;
        else 
        {
            AwardNames.push(header);
            if (i === Headers.length - 1 || Headers[i + 1].toLowerCase() !== '颜色') ACMap[header] = `#${Math.floor(Math.random() * 16777215).toString(16)}`;
        }
    }
    for (let j = 1; j < Data.length; j++)
    {
        const Row = Data[j];
        const Entity = Row[0];
        let AwardsData = [];
        for (let k = 0; k < AwardNames.length; k++) 
        {
            const AwardName = AwardNames[k];
            const AwardIndex = Headers.indexOf(AwardName);
            const AwardCount = Row[AwardIndex] || 0;
            AwardsData.push(
            {
                AwardName: AwardName,
                Count: AwardCount
            });
        }
        AwardArray.push(
        {
            Entity: Entity,
            AwardsData: AwardsData
        });
    }
    return { ACMap, AwardArray };
}

function EntitiesSorter(OriginArray, AwardName = null) 
{
    if (AwardName) 
    {
        const Index = OriginArray[0].AwardsData.findIndex(award => award.AwardName === AwardName);
        if (Index > -1) 
        {
            OriginArray.sort((a, b) => b.AwardsData[Index].Count - a.AwardsData[Index].Count);
            OriginArray.forEach(CurEntity => 
            {
                const AwardData = CurEntity.AwardsData;
                CurEntity.AwardsData = [AwardData[Index]].concat(AwardData.slice(0, Index), AwardData.slice(Index + 1));
            });
        }
    } 
    else 
    {
        OriginArray.sort((a, b) => 
        {
            const SumA = a.AwardsData.reduce((acc, cur) => acc + cur.Count, 0);
            const SumB = b.AwardsData.reduce((acc, cur) => acc + cur.Count, 0);
            return SumB - SumA;
        });
    }
    return OriginArray.slice(0, 10);
}

function SetupButtons(ACMap, AwardArray) 
{
    const ButtonsContainer = document.getElementById('buttons');
    ButtonsContainer.innerHTML = '';
    const SortByTotalButton = document.createElement('button');
    SortByTotalButton.textContent = '按总数排序';
    SortByTotalButton.style.backgroundColor = 'white';
    SortByTotalButton.onclick = function() 
    {
        const sortedData = EntitiesSorter(AwardArray);
        DrawChart(ACMap, sortedData);
    };
    ButtonsContainer.appendChild(SortByTotalButton);
    Object.keys(ACMap).forEach(awardName => 
    {
        const button = document.createElement('button');
        button.textContent = `按 ${awardName} 排序`;
        button.style.backgroundColor = ACMap[awardName];
        button.onclick = function() 
        {
            const sortedData = EntitiesSorter(AwardArray, awardName);
            DrawChart(ACMap, sortedData);
        };
        ButtonsContainer.appendChild(button);
    });
}
  
function DrawChart(ACMap, ArrayToDraw) 
{
    d3.select('#RankContainer').selectAll('svg').remove();
    const Width = window.innerWidth * 0.9;
    const Height = window.innerHeight * 0.8;
    const Margin = { top: 100, right: 20, bottom: 30, left: 50 };
    const Svg = d3.select('#RankContainer')
                  .append('svg')
                  .attr('width', Width + Margin.left + Margin.right)
                  .attr('height', Height + Margin.top + Margin.bottom)
                  .style('background-color', 'white')
                  .append('g')
                  .attr('transform', `translate(${Margin.left}, ${Margin.top})`);
    const xScale = d3.scaleBand()
                     .range([0, Width])
                     .domain(ArrayToDraw.map(d => d.Entity))
                     .padding(0.1); 
    const maxCount = d3.max(ArrayToDraw, d => d3.max(d.AwardsData, award => award.Count));
    const yScale = d3.scaleLinear()
                     .range([Height, 0])
                     .domain([0, maxCount]);
    Svg.append('g')
       .attr('transform', `translate(0, ${Height})`)
       .call(d3.axisBottom(xScale));
    Svg.append('g').call(d3.axisLeft(yScale));
    const legendPadding = 20;
    const legendItemHeight = 25;
    const legendItemWidth = 25;
    const fontSize = 16;
    let legendWidth = 0;
    const legend = Svg.append('g');
    Object.entries(ACMap).forEach(([awardName, color], index) => 
    {
        const legendItem = legend.append('g')
                                 .attr('transform', `translate(${legendWidth}, 0)`);
        legendItem.append('rect')
                  .attr('width', legendItemWidth)
                  .attr('height', legendItemHeight)
                  .attr('fill', color);
        legendItem.append('text')
                  .attr('x', legendItemWidth + 5)
                  .attr('y', legendItemHeight / 2)
                  .attr('dy', '0.35em')
                  .style('font-size', `${fontSize}px`)
                  .text(awardName);
        legendWidth += legendItemWidth + 5 + awardName.length * (fontSize / 2) + legendPadding;
    });
    legend.attr('transform', `translate(${(Width - legendWidth) / 2}, -${Margin.top / 2})`);
    Svg.append('line')
       .attr('x1', (Width - legendWidth) / 2)
       .attr('x2', (Width + legendWidth) / 2)
       .attr('y1', -Margin.top / 2 + 40)
       .attr('y2', -Margin.top / 2 + 40)
       .attr('stroke', 'black')
       .attr('stroke-width', 1);
    ArrayToDraw.forEach((entity) => 
    {
        const awardWidth = xScale.bandwidth() / entity.AwardsData.length;
        const entityGroup = Svg.append('g')
                               .attr('transform', `translate(${xScale(entity.Entity)}, 0)`);
        entityGroup.selectAll('rect')
                   .data(entity.AwardsData)
                   .enter()
                   .append('rect')
                   .attr('x', (d, i) => i * awardWidth)
                   .attr('y', d => yScale(d.Count))
                   .attr('width', awardWidth)
                   .attr('height', d => Height - yScale(d.Count))
                   .attr('fill', d => ACMap[d.AwardName]);
        entityGroup.selectAll('text')
                   .data(entity.AwardsData)
                   .enter()
                   .append('text')
                   .attr('x', (d, i) => i * awardWidth + awardWidth / 2)
                   .attr('y', d => yScale(d.Count) - 5)
                   .attr('text-anchor', 'middle')
                   .style('font-size', '12px')
                   .text(d => d.Count);
    });
}

const hardcodedData = 
[
    ["分类", "金牌", "颜色", "银牌", "颜色", "铜牌", "颜色"],
    ["中国", 201, "#F3C35F", 111, "#DEE1E5", 71, "#C2B176"],
    ["日本", 52, "#F3C35F", 67, "#DEE1E5", 69, "#C2B176"],
    ["韩国", 42, "#F3C35F", 59, "#DEE1E5", 89, "#C2B176"],
    ["印度", 28, "#F3C35F", 38, "#DEE1E5", 41, "#C2B176"],
    ["乌兹别克斯坦", 22, "#F3C35F", 18, "#DEE1E5", 31, "#C2B176"],
    ["中华台北", 19, "#F3C35F", 20, "#DEE1E5", 28, "#C2B176"],
    ["伊朗", 13, "#F3C35F", 21, "#DEE1E5", 20, "#C2B176"],
    ["泰国", 12, "#F3C35F", 14, "#DEE1E5", 32, "#C2B176"],
    ["巴林", 12, "#F3C35F", 3, "#DEE1E5", 5, "#C2B176"],
    ["朝鲜", 11, "#F3C35F", 18, "#DEE1E5", 10, "#C2B176"],
    ["哈萨克斯坦", 10, "#F3C35F", 22, "#DEE1E5", 48, "#C2B176"],
    ["中国香港", 8, "#F3C35F", 16, "#DEE1E5", 29, "#C2B176"],
    ["印度尼西亚", 7, "#F3C35F", 11, "#DEE1E5", 18, "#C2B176"],
    ["马来西亚", 6, "#F3C35F", 8, "#DEE1E5", 18, "#C2B176"],
    ["卡塔尔", 5, "#F3C35F", 6, "#DEE1E5", 3, "#C2B176"]
];

document.addEventListener('DOMContentLoaded', function() 
{
    const { ACMap, AwardArray } = ProcessData(hardcodedData);
    const ArrayToDraw = EntitiesSorter(AwardArray);
    Temp(ACMap, ArrayToDraw, AwardArray);
});